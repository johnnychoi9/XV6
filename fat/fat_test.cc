#include "fat.h"
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <functional>
#include <algorithm>
#include <iostream>
#include <string>
#include <fstream>
#include <sstream>
#include <map>

namespace {
bool TEST_DEBUG = false;
int passed_groups = 0, failed_groups = 0;
int passed_total = 0, failed_total = 0;
int passed_in_group = 0, failed_in_group = 0;
std::string cwd = "/";
std::string saved_category = "";
std::string saved_subtest = "";

std::ofstream myout;


const int big_timeout = 200;

/* This testing script has an option to try fork()ing for each test,
   enabled by this #define. This allows timeouts for individual tests
   (in case of infinite loops) and recovery from crashes on
   individual tests which is handy when using this as a template for
   grading, but mostly confusing when using this for self-testing,
   so it is disabled by default

   The logic for this forking includes logic to save the offsets of
   every file descriptor >= 4 (fds 0 to 3 are stdin, stdout, stderr
   and 'myout') and restore it after waiting on the forked
   subprocess in order to handle cases where code uses the offset
   in a file descriptor to save something like the current working
   directory. (I do not recommmend this approach as it is extremely
   error-prone.)
 */
#ifdef DO_FORK_FOR_TESTS
int nest_level = 0;

std::map<int, off_t> save_fd_offsets() {
    std::map<int, off_t> fd_offsets;
    for (int i = 4; i < 1024; ++i) {
        off_t offset = lseek(i, 0, SEEK_CUR);
        if (offset != -1) {
            fd_offsets[i] = offset;
        }
    }
    return fd_offsets;
}

void restore_fd_offsets(std::map<int, off_t> fd_offsets) {
    for (int i = 0; i < 1024; ++i) {
        if (fd_offsets.count(i)) {
            lseek(i, fd_offsets[i], SEEK_SET);
        }
    }
}

void fork_and_run(std::function<void(void)> f, int timeout=-1) {
    ++nest_level;
    if (nest_level == 1 && timeout == -1) {
        timeout = 5;
    }
    if (nest_level == 2 && timeout == -1) {
        timeout = 3;
    }
    if (nest_level > 3 && timeout == 2) {
        timeout = 1;
    }
    auto offsets = save_fd_offsets();
    if (getenv("FAT_TEST_DO_NOT_FORK") != 0) {
        f();
    } else {
        sigset_t signals;
        sigemptyset(&signals);
        sigaddset(&signals, SIGCHLD);
        sigprocmask(SIG_BLOCK, &signals, NULL);
        myout << std::flush;
        int pid = fork();
        if (pid == 0) {
            sigprocmask(SIG_UNBLOCK, &signals, NULL);
            f();
            myout << std::flush;
            _Exit(0);
        } else {
            siginfo_t info;
            struct timespec to = {
                .tv_sec = (time_t) timeout,
                .tv_nsec = 0,
            };
            int rv = sigtimedwait(&signals, &info, &to);
            if (rv == -1 && errno == EAGAIN) {
                kill(pid, SIGKILL);
                myout << "TEST:TIMEOUT:\n";
                rv = sigtimedwait(&signals, &info, NULL);
                if (rv == -1)
                    abort();
            }
            int status;
            pid_t waited_pid = waitpid(pid, &status, 0);
            if (pid != waited_pid)
                abort();
            if (WIFSIGNALED(status)) {
                myout << "TEST:CRASH:\n";
                myout << std::flush;
            }
        }
        restore_fd_offsets(offsets);
    }
    --nest_level;
}
#else
void fork_and_run(std::function<void(void)> f, int timeout=-1) {
    f();
}
#endif


#define CHECK(x, what) \
    do { \
        myout << "    " << what << ": ";; \
        if (x) { \
            myout << "passed.\n"; \
            ++passed_in_group; \
            ++passed_total; \
        } else { \
            myout << "failed.\n"; \
            ++failed_in_group; \
            ++failed_total; \
        } \
    } while (0)

#ifdef GRADING_OUTPUT
#define TEST(x, category, subtest) \
    do { \
        myout << "TEST:"; \
        if (x) { \
            myout << "PASS:"; \
        } else { \
            myout << "FAIL:"; \
        } \
        myout << category << ":" << subtest << "\n"; \
        myout << std::flush; \
    } while (0)

#define START_TEST_SET(category, subtest) do { \
        passed_in_group = failed_in_group = 0; \
        if (saved_category != "") abort(); \
        saved_category = category; \
        saved_subtest = subtest; \
        myout << "TEST:START:" << category << ":" <<subtest << "\n"; \
        myout << std::flush; \
    } while (0)

#define CHECK_TEST_SET() do { \
        TEST(failed_in_group == 0, saved_category, saved_subtest); \
        passed_in_group = failed_in_group = 0; \
        saved_category = ""; \
        saved_subtest = ""; \
    } while (0)
#else
#define START_TEST_SET(category, subtest) do { \
        passed_in_group = failed_in_group = 0; \
        if (saved_category != "") abort(); \
        saved_category = category; \
        saved_subtest = subtest; \
        myout << "Running test group for " << category ": " << subtest << ":\n"; \
     } while (0)
#define CHECK_TEST_SET() do { \
        std::string result = failed_in_group == 0 ? "PASSED" : "FAILED"; \
        if (failed_in_group == 0) { ++passed_groups; } else { ++failed_groups; } \
        myout << result << " tests for " << saved_category << ": " << saved_subtest << ".\n" \
              << "(" << passed_in_group << " subtests passed and " << failed_in_group << " subtests failed.)\n\n"; \
        saved_category = ""; \
     } while (0)
#endif

#define THISTLE_TEXT \
    "All problems become smaller if you don't dodge them,\n"\
    "but confront them.\n"\
    "Touch a thistle timidly, and it pricks you;\n"\
    "grasp it boldly, and its spines crumble.\n"\
    "\n"\
    "William S. Halsey\n"\
    "\n"\

#define CONGRATS_TEXT  \
    "Congratulations, you have gotten something working!\n"\
    "\n"\
    "Now that you can wade through the FAT with the greatest of ease, you can\n"\
    "undercut all of those $30 shareware \"Undelete\" applications that market\n"\
    "to people with digital camera and USB drive dysfunctions.  Go forth and\n"\
    "take advantage of other people's misfortunes!\n"\
    "\n"\
    "Only $19.99!!!!\n"\
    "\n"\
    "-Duane\n"\
    "\n"

#define THE_GAME_TEXT \
    "Subject: for your amusement\n" \
    "To: csfaculty@uvacs.cs.virginia.edu\n" \
    "Date: Thu, 23 Mar 1995 10:49:33 -0500 (EST)\n" \
    "Content-Type: text/plain; charset=US-ASCII\n" \
    "Content-Transfer-Encoding: 7bit\n" \
    "Status: RO\n" \
    "Content-Length: 4226\n" \
    "\n" \
    "> Software - How Software Companies Die\n" \
    ">         By: Orson Scott Card\n" \
    "> \n" \
    "> The environment that nutures creative programmers kills management\n" \
    "> and marketing types - and vice versa.  Programming is the Great Game.\n" \
    "> It consumes you, body and soul.  When you're caught up in it, nothing\n" \
    "> else matters.  When you emerge into daylight, you might well discover\n" \
    "> that you're a hundred pounds overweight, your underwear is older than\n" \
    "> the average first grader, and judging from the number of pizza boxes\n" \
    "> lying around, it must be spring already.  But you don't care, because\n" \
    "> your program runs, and the code is fast and clever and tight.  You won.\n" \
    "> You're aware that some people think you're a nerd.  So what?  They're\n" \
    "> not players.  They've never jousted with Windows or gone hand to hand\n" \
    "> with DOS. To them C++ is a decent grade, almost a B - not a language.\n" \
    "> They barely exist.  Like soldiers or artists, you don't care about the\n" \
    "> opinions of civilians.  You're building something intricate and fine.\n" \
    "> They'll never understand it.\n" \
    "> \n" \
    "> BEEKEEPING\n" \
    "> \n" \
    "> Here's the secret that every successful software company is based on:\n" \
    "> You can domesticate programmers the way beekeepers tame bees.  You\n" \
    "> can't exactly communicate with them, but you can get them to swarm in\n" \
    "> one place and when they're not looking, you can carry off the honey.\n" \
    "> You keep these bees from stinging by paying them money.  More money\n" \
    "> than they know what to do with.  But that's less than you might think.\n" \
    "> You see, all these programmers keep hearing their parents' voices in\n" \
    "> their heads saying \"When are you going to join the real world?\"  All\n" \
    "> you have to pay them is enough money that they can answer (also in\n" \
    "> their heads) \"Geez, Dad, I'm making more than you.\"  On average, this\n" \
    "> is cheap.  And you get them to stay in the hive by giving them other\n" \
    "> coders to swarm with.  The only person whose praise matters is another\n" \
    "> programmer.  Less-talented programmers will idolize them; evenly\n" \
    "> matched ones will challenge and goad one another; and if you want to\n" \
    "> get a good swarm, you make sure that you have at least one certified\n" \
    "> genius coder that they can all look up to, even if he glances at other\n" \
    "> people's code only long enough to sneer at it. He's a Player, thinks\n" \
    "> the junior programmer.  He looked at my code.  That is enough. If a\n" \
    "> software company provides such a hive, the coders will give up sleep,\n" \
    "> love, health, and clean laundry, while the company keeps the bulk of\n" \
    "> the money.\n" \
    "> \n" \
    "> OUT OF CONTROL\n" \
    "> \n" \
    "> Here's the problem that ends up killing company after company.  All\n" \
    "> successful software companies had, as their dominant personality, a\n" \
    "> leader who nurtured programmers.  But no company can keep such a leader\n" \
    "> forever. Either he cashes out, or he brings in management types who end\n" \
    "> up driving him out, or he changes and becomes a management type himself.\n" \
    "> One way or another, marketers get control.  But...control of what?\n" \
    "> Instead of finding assembly lines of productive workers, they quickly\n" \
    "> discover that their product is produced by utterly unpredictable,\n" \
    "> uncooperative, disobedient, and worst of all, unattractive people who\n" \
    "> resist all attempts at management.  Put them on a time clock, dress\n" \
    "> them in suits, and they become sullen and start sabotaging the product.\n" \
    "> Worst of all, you can sense that they are making fun of you with every\n" \
    "> word they say.\n" \
    "> \n" \
    "> SMOKED OUT\n" \
    "> \n" \
    "> The shock is greater for the coder, though.  He suddenly finds that\n" \
    "> alien creatures control his life.  Meetings, Schedules, Reports.  And\n" \
    "> now someone demands that he PLAN all his programming and then stick to\n" \
    "> the plan, never improving, never tweaking, and never, never touching\n" \
    "> some other team's code. The lousy young programmer who once worshiped\n" \
    "> him is now his tyrannical boss, a position he got because he played\n" \
    "> golf with some sphincter in a suit. The hive has been ruined.  The best\n" \
    "> coders leave.  And the marketers, comfortable now because they're\n" \
    "> surrounded by power neckties and they have things under control, are\n" \
    "> baffled that each new iteration of their software loses market share\n" \
    "> as the code bloats and the bugs proliferate.  Got to get some better\n" \
    "> packaging.  Yeah, that's it.\n" \
    "\n"


bool is_long_name(const AnyDirEntry &entry) {
    return (entry.dir.DIR_Attr & DirEntryAttributes::LONG_NAME_MASK) == DirEntryAttributes::LONG_NAME;
}

void _check_root_dir(const std::string &path, bool hard = true) {
    START_TEST_SET("readdir of root dir", "cwd=" + cwd + ",path=" + path);
    bool saw_people = false;
    bool saw_congrats = false;
    bool saw_example1 = false;
    bool saw_examp999 = false;
    bool saw_a1 = false;
    bool saw_a2 = false;
    bool saw_a3 = false;
    bool saw_a99 = false;
    for (AnyDirEntry entry : fat_readdir(path)) {
        if (!is_long_name(entry)) {
            std::string name((char*) entry.dir.DIR_Name, (char*) entry.dir.DIR_Name + 11);
            if (name == "PEOPLE     ") {
                saw_people = true;
                CHECK(entry.dir.DIR_Attr & DirEntryAttributes::DIRECTORY, "people is a directory");
            } else if (name == "CONGRATSTXT") {
                saw_congrats = true;
            } else if (name == "EXAMPLE1TXT") {
                saw_example1 = true;
            } else if (name == "EXAMP999TXT") {
                saw_examp999 = true;
            } else if (name == "A1         ") {
                saw_a1 = true;
            } else if (name == "A2         ") {
                saw_a2 = true;
            } else if (name == "A3         ") {
                saw_a3 = true;
            } else if (name == "A99        ") {
                saw_a99 = true;
            }
        }
    }
    CHECK(saw_people, "people directory was found in " << path);
    CHECK(saw_congrats, "congrats.txt was found in " << path);
    CHECK(saw_a1, "a1 was found in " << path);
    CHECK(saw_example1, "example1 was found in " << path);
    CHECK_TEST_SET();
    if (hard) {
        START_TEST_SET("readdir of root dir -- past 0xE5", "cwd=" + cwd + ",path=" + path);
        CHECK(saw_a3 && saw_a99, "A3 and A99 was found in " << path);
        CHECK_TEST_SET();
        START_TEST_SET("readdir of root dir -- multiple clusters", "cwd=" + cwd + ",path=" + path);
        CHECK(saw_a2 && saw_examp999, "A2 and EXAMP999.TXT was found in " << path);
        CHECK_TEST_SET();
    }
}
void check_root_dir(const std::string &path, bool hard = false) {
    fork_and_run(std::bind(&_check_root_dir, path, hard));
}

void _check_yyz5w_dir(const std::string &path) {
    START_TEST_SET("readdir of yyz5w", "cwd=" + cwd + ",path=" + path);
    bool saw_the_game_txt = false;
    bool saw_extra = false;
    for (AnyDirEntry entry : fat_readdir(path)) {
        if (entry.dir.DIR_Name[0] == 0)
            break;
        if (entry.dir.DIR_Name[0] == 0xE5)
            continue;
        if (!is_long_name(entry) && !(entry.dir.DIR_Attr & (DirEntryAttributes::SYSTEM | DirEntryAttributes::HIDDEN))) {
            std::string name((char*) entry.dir.DIR_Name, (char*) entry.dir.DIR_Name + 11);
            if (name == "THE-GAMETXT") {
                saw_the_game_txt = true;
            } else if (name == ".          ") {
            } else if (name == "..         ") {
            } else {
                saw_extra = true;
            }
        }
    }
    CHECK(saw_the_game_txt, "the-game.txt found in " << path);
    CHECK(!saw_extra, "extra NOT found in " << path);
    CHECK_TEST_SET();
}

void check_yyz5w_dir(const std::string &path) {
    fork_and_run(std::bind(&_check_yyz5w_dir, path));
}

void _check_people_dir(const std::string &path) {
    START_TEST_SET("readdir of people", "cwd=" + cwd + ",path=" + path);
    bool saw_yyz5w = false;
    bool saw_smb3wk = false;
    bool saw_example2 = false;
    bool saw_congrats = false;
    for (AnyDirEntry entry : fat_readdir(path)) {
        if (!is_long_name(entry)) {
            std::string name((char*) entry.dir.DIR_Name, (char*) entry.dir.DIR_Name + 11);
            if (name == "YYZ5W      ") {
                saw_yyz5w = true;
            } else if (name == "SMB3WK     ") {
                saw_smb3wk = true;
            } else if (name == "EXAMPLE2TXT") {
                saw_example2 = true;
            } else if (name == "CONGRATSTXT") {
                saw_congrats = true;
            }
        }
    }
    CHECK(saw_example2, "example2 found in " << path);
    CHECK(saw_yyz5w, "yyz5w found in " << path);
    CHECK(saw_smb3wk, "smb3wk found in " << path);
    CHECK(!saw_congrats, "congrats.txt NOT in " << path);
    CHECK_TEST_SET();
}

void check_people_dir(const std::string &path) {
    fork_and_run(std::bind(&_check_people_dir, path));
}

void _check_a99(const std::string &path) {
    bool saw_foo_xx = false;
    START_TEST_SET("readdir of a99", "cwd=" + cwd + ",path=" + path);
    for (AnyDirEntry entry : fat_readdir(path)) {
        if (!is_long_name(entry)) {
            std::string name((char*) entry.dir.DIR_Name, (char*) entry.dir.DIR_Name + 11);
            if (name == "FOO     XX ") {
                saw_foo_xx = true;
            }
        }
    }
    CHECK(saw_foo_xx, "foo.xx found in " << path);
    CHECK_TEST_SET();
}

void check_a99(const std::string &path) {
    fork_and_run(std::bind(&_check_a99, path));
}

void _check_a2(const std::string &path) {
    START_TEST_SET("readdir of a2", "cwd=" + cwd + ",path=" + path);
    bool saw_example3 = false;
    for (AnyDirEntry entry : fat_readdir(path)) {
        if (!is_long_name(entry)) {
            std::string name((char*) entry.dir.DIR_Name, (char*) entry.dir.DIR_Name + 11);
            if (name == "EXAMPLE3TXT") {
                saw_example3 = true;
            }
        }
    }
    CHECK(saw_example3, "example3 found in " << path);
    CHECK_TEST_SET();
}

void check_a2(const std::string &path) {
    fork_and_run(std::bind(&_check_a2, path));
}

void _check_b4(const std::string &path) {
    START_TEST_SET("readdir of b4", "cwd=" + cwd + ",path=" + path);
    bool saw_example8 = false;
    bool saw_example9 = false;
    for (AnyDirEntry entry : fat_readdir(path)) {
        if (!is_long_name(entry)) {
            std::string name((char*) entry.dir.DIR_Name, (char*) entry.dir.DIR_Name + 11);
            if (TEST_DEBUG) myout << "CHECKING [" << name << "]" << std::endl;
            if (name == "EXAMPLE9TXT") {
                saw_example9 = true;
            } else if (name == "EXAMPLE8TXT") {
                saw_example8 = true;
            }
        }
    }
    CHECK(saw_example8, "example8 found in " << path);
    CHECK(saw_example9, "example9 found in " << path);
    CHECK_TEST_SET();
}

void check_b4(const std::string &path) {
    fork_and_run(std::bind(&_check_b4, path));
}


void check_pread_ranges(int fd, const std::string &expected, const std::string &scenario, bool check_excess = true) {
    if (check_excess) {
        START_TEST_SET("pread with various offsets and counts, contents/size/not exceeding buffer", scenario);
    } else {
        START_TEST_SET("pread with various offsets and counts, contents/size", scenario);
    }
    if (fd == -1) {
        CHECK(false, "fd for pread is -1");
    } else {
        char buffer[16 * 1024 + 512];
        for (unsigned raw_offset = 0; raw_offset < expected.size(); raw_offset += 1024) {
            for (unsigned offset = raw_offset - 1; offset <= raw_offset + 1; ++offset) {
                for (unsigned raw_count = 0; raw_count < expected.size() && raw_count < sizeof(buffer) - 4; raw_count += 1024) {
                    for (unsigned count = raw_count - 2; count <= raw_count + 2; ++count) {
                        memset(buffer, 'X', sizeof buffer);
                        int read_count = fat_pread(fd, (void*) buffer, count, offset);
                        int expected_count = std::max(0, std::min((int) count, (int) (expected.size() - offset)));
                        CHECK(read_count == expected_count, "pread return value, offset=" << offset << ",count=" << count 
                                << ",expected=" << expected_count << ",actual=" << read_count);
                        CHECK(std::string(buffer, buffer + expected_count) == expected.substr(offset, expected_count), "pread contents");
                        if (check_excess)
                            CHECK(*(buffer + expected_count + 4) == 'X', "pread does not go past end of buffer");
                    }
                }
            }
        }
    }
    CHECK_TEST_SET();
}

void _check_pread_simple(int fd, const std::string &expected, const std::string &scenario, bool include_size = false) {
    std::vector<char> buffer;
    if (fd == -1) {
        CHECK(false, scenario << ": no fd");
    }
    buffer.resize(expected.size() + 4096);
    buffer[expected.size()] = 'X';
    int read_count = fat_pread(fd, &buffer[0], include_size ? expected.size() + 1024 : expected.size(), 0);
    CHECK(std::string(buffer.begin(), buffer.begin() + expected.size()) == expected, scenario << ": contents");
    if (std::string(buffer.begin(), buffer.begin() + expected.size()) != expected) {
        myout << "Got text [" << std::string(buffer.begin(), buffer.begin() + expected.size()) << "]" << std::endl;
        myout << "Expected text [" << std::string(expected.begin(), expected.begin() + expected.size()) << "]" << std::endl;
    }
    if (include_size) {
        CHECK(read_count == (int) expected.size(), scenario << ": size");
        CHECK(buffer[expected.size()] == 'X', scenario << ": no writing from past end of file");
    }
}

void _check_contents(const std::string &path, const std::string &contents, bool only_simple = true) {
    std::string scenario = "path="+path+",cwd=" + cwd;
    START_TEST_SET("simple open+read", scenario);
    int fd = fat_open(path);
    CHECK(fd >= 0, "opening " << path);
    if (fd >= 0) {
        _check_pread_simple(fd, contents, scenario, true);
    }
    CHECK_TEST_SET();
    if (fd >= 0) {
        fat_close(fd);
    }
    START_TEST_SET("simple open+read+size", scenario);
    fd = fat_open(path);
    CHECK(fd >= 0, "opening " << path);
    if (fd >= 0) {
        _check_pread_simple(fd, contents, scenario, false);
    }
    CHECK_TEST_SET();
    if (!only_simple) {
        check_pread_ranges(fd, contents, scenario, true);
        check_pread_ranges(fd, contents, scenario, false);
    }
    if (fd >= 0) {
        fat_close(fd);
    }
}

void check_contents(const std::string &path, const std::string &contents, bool only_simple = true) {
    fork_and_run(
        std::bind(&_check_contents, path, contents, only_simple)
    );
}

void _check_not_openable(const std::string &path, const std::string & scenario = "") {
    START_TEST_SET("failed open", scenario);
    int fd = fat_open(path);
    CHECK(fd == -1, "opening " << path << " should fail");
    if (fd >= 0) {
        fat_close(fd);
    }
    CHECK_TEST_SET();
}

void check_not_openable(const std::string &path, const std::string & scenario) {
    fork_and_run(
        std::bind(&_check_not_openable, path, scenario)
    );
}

void _check_not_readdir(const std::string & path, const std::string & scenario) {
    START_TEST_SET("failed readdir", scenario);
    CHECK(fat_readdir(path).size() == 0, "cannot readdir " << path);
    CHECK_TEST_SET();
}

void check_not_readdir(const std::string &path, const std::string & scenario) {
    fork_and_run(
        std::bind(&_check_not_readdir, path, scenario)
    );
}

void _check_not_cd(const std::string & path, const std::string & scenario) {
    START_TEST_SET("failed cd", scenario);
    CHECK(!fat_cd(path), "cannot cd " << path);
    CHECK_TEST_SET();
}

void check_not_cd(const std::string &path, const std::string & scenario) {
    fork_and_run(
        std::bind(&_check_not_cd, path, scenario)
    );
}

bool check_cd(const std::string & path, const std::string & new_cwd) {
    START_TEST_SET("succesful cd", "path=" + path + ",new_cwd=" + new_cwd);
    bool result = fat_cd(path);
    CHECK(result, "cd " << path);
    CHECK_TEST_SET();
    cwd = new_cwd;
#if 0
    return result;
#else
    return true;    // assume success
#endif
}

void a1_test(void) {
    if (check_cd("a1", "/a1")) {
        check_b4("b1/b2/b3/b4");
        check_contents("b1/b2/b3/b4/example9.txt", "This is example 9.\n");
    }
}

void a2_test(void) {
    if (check_cd("a2","/a2")) {
        check_a2(".");
        check_contents("example3.txt", "the contents of example3.txt\n");
        check_contents("../a1/b1/b2/example5.txt", "This is example 5.\n");
    }
}

void mounted_tests(void) {
    bool mounted = fat_mount("testdisk1.raw");
    cwd = "/";
    CHECK(mounted, "mounting testdisk1.raw successful");
    if (!mounted) {
        myout << "could not mount testdisk1.raw, skipping rest of tests\n";
        myout << "(is that file in this directory?)\n";
        myout << std::flush;
        _Exit(1);
    }
    check_root_dir("/");
    check_contents("/congrats.txt", CONGRATS_TEXT, false);
    check_contents("/congrAts.tXt", CONGRATS_TEXT);
    check_contents("congrats.tXt", CONGRATS_TEXT);
    check_contents("CONGRATS.TXT", CONGRATS_TEXT);
    check_contents("/example1.txt", "the contents of example1.\n");
    check_contents("gamecopy.txt", THE_GAME_TEXT, false);
    check_contents("gamecopy.txt", THE_GAME_TEXT);
    check_contents("gamefrag.txt", THE_GAME_TEXT, false);
    check_contents("gamefrag.txt", THE_GAME_TEXT);
    check_contents("foo.a", "simple file\n");
    check_contents("foo.ba", "simple file 2\n");
    check_people_dir("/people");
    check_contents("people/example2.txt", "The contents of example2.\n");
    check_contents("/people/example2.txt", "The contents of example2.\n");
    check_a2("/a2");
    check_a2("a2");
    check_a99("/a99");
    check_a99("a99");
    check_contents("a99/foo.xx", "simple file 3\n");
    check_contents("/a99/foo.xx", "simple file 3\n");
    check_contents("a99/congrats.txt", CONGRATS_TEXT);
    check_contents("a99/new-game.txt", THE_GAME_TEXT);
    check_not_cd("xysdfd", "cd to directory that does not exist");
    check_not_cd("yyz5w", "cd to directory that is elsewhere");
    check_contents("/example1.txt", "the contents of example1.\n");
    check_contents("example1.txt", "the contents of example1.\n");
    check_contents("/a2/example3.txt", "the contents of example3.txt\n");
    check_root_dir("/people/..");
    check_root_dir("/PEoPLE/..");
    check_root_dir("/Media/..");
    check_contents("/a1/b1/b2/b3/b4/example9.txt", "This is example 9.\n");
    check_contents("/a1/b1/b2/b3/example6.txt", "This is example 6.\n");
    check_b4("/a1/b1/b2/b3/b4");
    check_contents("people/../congrats.txt", CONGRATS_TEXT);
    check_contents("people/../people/../people/../people/../congrats.txt", CONGRATS_TEXT);
    check_contents("/people/yyz5w/the-game.txt", THE_GAME_TEXT, false);
    check_contents("people/yyz5w/the-game.txt", THE_GAME_TEXT);
    check_people_dir("/media/../people/smb3wk/..");
    check_people_dir("/people/smb3wk/..");
    check_people_dir("/people/yyz5w/..");
    check_yyz5w_dir("/people/yyz5w");
    check_yyz5w_dir("/people/yyz5w/../yyz5w");
    START_TEST_SET("multiple file descriptors", "");
    int fd_one = fat_open("congrats.txt");
    int fd_two = fat_open("example1.txt");
    _check_pread_simple(fd_one, CONGRATS_TEXT, "congrats.txt");
    _check_pread_simple(fd_two, "the contents of example1.\n", "example1.txt");
    fat_close(fd_one);
    fat_close(fd_two);
    CHECK_TEST_SET();
    if (check_cd("people", "/people")) {
        check_contents("rat2z/thistle.txt", THISTLE_TEXT);
        check_yyz5w_dir("yyz5w");
        check_people_dir(".");
        check_root_dir("..");
        check_root_dir("/");
        check_root_dir("/people/..");
        check_a99("../a99");
        check_not_openable("congrats.txt", "file in root while cd'd");
        check_not_cd("people", "cd to directory in root while cd'd");
        check_not_openable("the-game.txt", "file in nested directory while cd'd");
        check_contents("yyz5w/the-game.txt", THE_GAME_TEXT);
        check_contents("./yyz5w/the-game.txt", THE_GAME_TEXT);
        check_contents("./yYZ5w/the-game.txt", THE_GAME_TEXT);
        check_contents("/people/yyz5w/the-game.txt", THE_GAME_TEXT);
        check_contents("/congrats.txt", CONGRATS_TEXT);
        check_contents("./example2.txt", "The contents of example2.\n");
        check_contents("example2.txt", "The contents of example2.\n");
        check_contents("../congrats.txt", CONGRATS_TEXT);
        if (check_cd("yyz5w", "/people/yyz5w")) {
            check_yyz5w_dir(".");
            check_root_dir("../..");
            check_people_dir("..");
            check_contents("../example2.txt", "The contents of example2.\n");
            check_people_dir("../../media/../people/smb3wk/..");
            check_people_dir("../smb3wk/..");
            check_contents("the-game.txt", THE_GAME_TEXT);
            check_contents("./the-game.txt", THE_GAME_TEXT);
            check_contents("../yyz5w/the-game.txt", THE_GAME_TEXT);
            check_contents("../../congrats.txt", CONGRATS_TEXT);
            check_not_openable("../congrats.txt", "file in root directory via .. while double-cd'd");
        }
    }
    if (check_cd("/", "/ (reset after cd /)")) {
        check_contents("congrats.txt", CONGRATS_TEXT);
        check_not_openable("the-game.txt", "the-game.txt after cd'ing to root");
    }
    fork_and_run(a1_test);
    check_cd("/", "/ (reset after cd /)");
    fork_and_run(a2_test);
    check_cd("/", "/ (reset after cd /)");
}

void premount_tests() {
    check_not_openable("/congrats.txt", "opening file before mount");
    check_not_readdir("/", "readdir before mount");
    check_not_cd("people", "cd before mount");
}
}

int main(int argc, char **argv) {
    myout.open("/dev/fd/1");
#ifdef HIDE_STDOUT
    int fd = open("/dev/null", O_RDWR);
    dup2(fd, 1);
    dup2(fd, 2);
#endif
    fork_and_run(premount_tests, big_timeout);
    mounted_tests();
#ifndef GRADING_OUTPUT
    myout << "Passed " << passed_groups << " test groups and failed " << failed_groups << " test groups\n";
    myout << "(by passing " << passed_total << " subtests and failing " << failed_total << " subtests)\n\n";
    myout << "Note that the number of tests/subtests is not a very good indication of how\n"
             "much you've completed; for example there several tests reading\n"
             "from many offsets in a file; all of these will likely only be one\n"
             "item on our grading rubric, while failing a small number of tests\n"
             "related to reading the root directory indicates a more serious\n"
             "problem.\n\n"
             "Also, please be aware we may run additional tests, including tests with\n"
             "a different disk image for grading.\n" << std::endl;
#endif
}
