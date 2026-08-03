// SPDX-FileCopyrightText:  2026 Antigravity
// SPDX-FileCopyrightText:  2026 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#include "dos/dos_append.h"

#include <filesystem>
#include <string>

#include <gtest/gtest.h>

#include "cpu/registers.h"
#include "dos/dos.h"
#include "dos/programs/append.h"
#include "shell/command_line.h"
#include "shell/shell.h"

#include "dos/dos_system.h"
#include "dos/drives.h"
#include "dosbox_test_fixture.h"

#include "hardware/memory.h"
#include "misc/logging.h"


namespace {

class DosAppendTest : public DOSBoxTestFixture {
protected:
	void SetUp() override
	{
		DOSBoxTestFixture::SetUp();

		LOG_MSG("Mount C drive to static test fixture directory tests/files/append/");
		Drives[2] = std::make_shared<localDrive>("tests/files/append/", 512, 1, 1, 1, 1, false);
		DOS_SetDefaultDrive(2);

		LOG_MSG("Setup test PSP and environment segment in DOS memory");
		uint16_t psp_seg = 0x2000;
		uint16_t env_seg = 0x2012;
		DOS_MCB pspmcb((uint16_t)(psp_seg - 1));
		pspmcb.SetPSPSeg(psp_seg);
		pspmcb.SetSize(0x10 + 2);
		pspmcb.SetType(0x4d);
		DOS_MCB envmcb((uint16_t)(env_seg - 1));
		envmcb.SetPSPSeg(psp_seg);
		envmcb.SetSize(0x100);
		envmcb.SetType(0x4d);
		mem_writeb(PhysicalMake(env_seg, 0), 0);
		DOS_PSP psp(psp_seg);
		psp.MakeNew(0);
		psp.SetEnvironment(env_seg);
		dos.psp(psp_seg);

		LOG_MSG("Ensure APPEND is clear before each test");
		dos_append::SetDirectories("");
		dos_append::SetFlags(false, true, false);
	}

	void TearDown() override
	{
		LOG_MSG("Tearing down the test environment");
		LOG_MSG("Resetting the drives list");
		Drives[2].reset();
		dos_append::SetDirectories("");
		dos.psp(0);

		LOG_MSG("Resetting the dos test fixture");
		DOSBoxTestFixture::TearDown();
	}
};

// ================================================================================
// UNIT TESTS: State Management
// ================================================================================

// Requirement: Verify initial default state is disabled with empty directory list. Target: dos_append::IsEnabled(), dos_append::GetDirectories()
TEST_F(DosAppendTest, Initialization)
{
	EXPECT_FALSE(dos_append::IsEnabled());
	EXPECT_EQ(dos_append::GetDirectories(), "");
}

// Requirement: Verify setting non-empty directory list enables APPEND resolution. Target: dos_append::SetDirectories(), dos_append::IsEnabled()
TEST_F(DosAppendTest, Activation)
{
	dos_append::SetDirectories("C:\\DIR");
	EXPECT_TRUE(dos_append::IsEnabled());
}

// Requirement: Verify setting empty directory list disables APPEND resolution. Target: dos_append::SetDirectories(), dos_append::IsEnabled()
TEST_F(DosAppendTest, Deactivation)
{
	dos_append::SetDirectories("C:\\DIR");
	EXPECT_TRUE(dos_append::IsEnabled());

	dos_append::SetDirectories("");
	EXPECT_FALSE(dos_append::IsEnabled());
	EXPECT_EQ(dos_append::GetDirectories(), "");
}

// Requirement: Verify consecutive APPEND commands replace previous directory list. Target: dos_append::SetDirectories(), dos_append::GetDirectories()
TEST_F(DosAppendTest, ReplacementBehavior)
{
	dos_append::SetDirectories("C:\\ONE");
	EXPECT_EQ(dos_append::GetDirectories(), "C:\\ONE");

	// Consecutive calls completely replace the list
	dos_append::SetDirectories("C:\\TWO");
	EXPECT_EQ(dos_append::GetDirectories(), "C:\\TWO");
}

// ================================================================================
// UNIT TESTS: Parser and Normalization
// ================================================================================

// Requirement: Verify raw trailing directory backslashes are preserved in list storage. Target: dos_append::SetDirectories(), dos_append::GetDirectories()
TEST_F(DosAppendTest, ParserTrailingSeparators)
{
	dos_append::SetDirectories("C:\\DIR\\");
	EXPECT_EQ(dos_append::GetDirectories(), "C:\\DIR\\");
}

// Requirement: Verify semicolon argument clears directory list. Target: APPEND::Run(), dos_append::SetDirectories()
TEST_F(DosAppendTest, ParserEmptyClear)
{
	dos_append::SetDirectories("C:\\DIR");
	auto* cmd = new CommandLine("APPEND", ";");
	APPEND prog;
	prog.cmd = cmd;
	prog.Run();
	EXPECT_EQ(dos_append::GetDirectories(), "");
	EXPECT_FALSE(dos_append::IsEnabled());
}

// Requirement: Verify duplicate directory entries in command string are preserved. Target: APPEND::Run(), dos_append::GetDirectories()
TEST_F(DosAppendTest, ParserDuplicates)
{
	DOS_MakeDir("C:\\ONE");
	DOS_MakeDir("C:\\TWO");
	auto* cmd = new CommandLine("APPEND", "C:\\ONE;C:\\ONE;C:\\TWO");
	APPEND prog;
	prog.cmd = cmd;
	prog.Run();
	EXPECT_EQ(dos_append::GetDirectories(), "C:\\ONE;C:\\ONE;C:\\TWO");
}

// Requirement: Verify option switches are stripped from directory list parsing. Target: APPEND::Run(), parse_options()
TEST_F(DosAppendTest, ParserSwitches)
{
	DOS_MakeDir("C:\\DIR");
	auto* cmd = new CommandLine("APPEND", "/X:ON /PATH:OFF C:\\DIR /X");
	APPEND prog;
	prog.cmd = cmd;
	prog.Run();
	EXPECT_EQ(dos_append::GetDirectories(), "C:\\DIR");
}

// Requirement: Verify /X:ON and /PATH:OFF flags set corresponding subsystem flags. Target: dos_append::SetFlags(), IsExecOn(), IsPathOverrideOn()
TEST_F(DosAppendTest, ParserFlags)
{
	DOS_MakeDir("C:\\DATA");
	auto* cmd = new CommandLine("APPEND", "/X:ON /PATH:OFF C:\\DATA");
	APPEND prog;
	prog.cmd = cmd;
	prog.Run();

	EXPECT_TRUE(dos_append::IsExecOn());
	EXPECT_FALSE(dos_append::IsPathOverrideOn());
	// We didn't pass /E, so list should be saved normally.
	EXPECT_EQ(dos_append::GetDirectories(), "C:\\DATA");
}

// Requirement: Verify whitespace and surrounding quotes are trimmed from paths. Target: dos_append::ValidateDirectories()
TEST_F(DosAppendTest, ParserWhitespaceAndQuotes)
{
	DOS_MakeDir("C:\\ONE");
	DOS_MakeDir("C:\\TWO");
	DOS_MakeDir("C:\\DIR1");
	auto* cmd = new CommandLine("APPEND", " C:\\ONE ; \"C:\\DIR1\" ;  C:\\TWO ");
	APPEND prog;
	prog.cmd = cmd;
	prog.Run();
	EXPECT_EQ(dos_append::GetDirectories(), "C:\\ONE;C:\\DIR1;C:\\TWO");
}

// Requirement: Verify empty token strings between multiple semicolons are ignored. Target: dos_append::ValidateDirectories()
TEST_F(DosAppendTest, ParserEmptyTokens)
{
	DOS_MakeDir("C:\\ONE");
	DOS_MakeDir("C:\\TWO");
	auto* cmd = new CommandLine("APPEND", "C:\\ONE;;;C:\\TWO;");
	APPEND prog;
	prog.cmd = cmd;
	prog.Run();
	EXPECT_EQ(dos_append::GetDirectories(), "C:\\ONE;C:\\TWO");
}

// Requirement: Verify relative directory paths expand to absolute DOS drive paths. Target: dos_append::ValidateDirectories()
TEST_F(DosAppendTest, ParserAbsoluteExpansion)
{
	DOS_MakeDir("C:\\TEST");
	DOS_MakeDir("C:\\TEST\\DATA");

	DOS_SetDefaultDrive(2);
	DOS_ChangeDir("TEST");

	auto* cmd = new CommandLine("APPEND", "DATA");
	APPEND prog;
	prog.cmd = cmd;
	prog.Run();

	EXPECT_EQ(dos_append::GetDirectories(), "C:\\TEST\\DATA");

	DOS_ChangeDir("\\");
}

// Requirement: Verify invalid directory input aborts without mutating existing list. Target: dos_append::ValidateDirectories()
TEST_F(DosAppendTest, ParserInvalidPath)
{
	dos_append::SetDirectories("C:\\GOOD");

	auto* cmd = new CommandLine("APPEND", "C:\\NONEXISTENT");
	APPEND prog;
	prog.cmd = cmd;
	prog.Run();

	EXPECT_EQ(dos_append::GetDirectories(), "C:\\GOOD");
}

// Requirement: Verify optional leading '=' sign is stripped during command parsing. Target: dos_append::ValidateDirectories()
TEST_F(DosAppendTest, ParserEqualSign)
{
	DOS_MakeDir("C:\\DATA");
	auto* cmd = new CommandLine("APPEND", "=C:\\DATA");
	APPEND prog;
	prog.cmd = cmd;
	prog.Run();

	EXPECT_EQ(dos_append::GetDirectories(), "C:\\DATA");
}

// ================================================================================
// UNIT TESTS: Path Resolution (ResolveName)
// ================================================================================

// Requirement: Verify path resolution returns false when APPEND list is empty. Target: dos_append::find_absolute_path()
TEST_F(DosAppendTest, ResolveNameDisabledState)
{
	dos_append::SetDirectories("");
	std::string out_path;
	EXPECT_FALSE(dos_append::find_absolute_path("FILE.TXT", out_path));
}

// Requirement: Verify basename extraction resolves simple filenames against appended directories. Target: dos_append::find_absolute_path()
TEST_F(DosAppendTest, ResolveNameBasenameExtraction)
{
	dos_append::SetDirectories("C:\\DIR");

	DOS_MakeDir("C:\\DIR");
	uint16_t entry;
	DOS_CreateFile("C:\\DIR\\README.TXT", 0, &entry);
	DOS_CloseFile(entry);

	std::string out_path;

	// Extraction test: Absolute paths or paths with directories should
	// bypass APPEND when /PATH:OFF is set
	dos_append::SetFlags(false, false, false);
	EXPECT_FALSE(dos_append::find_absolute_path("D:\\OTHER\\README.TXT", out_path));

	// Pure filenames should be resolved
	EXPECT_TRUE(dos_append::find_absolute_path("README.TXT", out_path));
	EXPECT_EQ(out_path, "C:\\DIR\\README.TXT");
}

// Requirement: Verify left-to-right directory search order and trailing slash normalization. Target: dos_append::find_absolute_path()
TEST_F(DosAppendTest, ResolveNameOrderingAndNormalization)
{
	DOS_MakeDir("C:\\ONE");
	DOS_MakeDir("C:\\TWO");
	DOS_MakeDir("C:\\THREE");

	uint16_t entry;
	DOS_CreateFile("C:\\ONE\\FILE.TXT", 0, &entry);
	DOS_CloseFile(entry);
	DOS_CreateFile("C:\\THREE\\FILE.TXT", 0, &entry);
	DOS_CloseFile(entry);

	dos_append::SetDirectories("c:\\two\\;;;C:\\ONE\\;C:\\THREE");

	std::string out_path;
	EXPECT_TRUE(dos_append::find_absolute_path("FILE.TXT", out_path));
	EXPECT_EQ(out_path, "C:\\ONE\\FILE.TXT");
}

// Requirement: Verify /PATH:ON searches APPEND list even when path is specified, /PATH:OFF bypasses. Target: dos_append::find_absolute_path(), SetFlags()
TEST_F(DosAppendTest, PathOverride)
{
	DOS_MakeDir("C:\\DIR");
	uint16_t entry;
	DOS_CreateFile("C:\\DIR\\README.TXT", 0, &entry);
	DOS_CloseFile(entry);

	dos_append::SetDirectories("C:\\DIR");

	std::string out_path;

	// By default, path override is true in DOS 4.0+.
	// Even if we provide an absolute path to a missing file, it should search APPEND!
	dos_append::SetFlags(false, true, false);
	EXPECT_TRUE(dos_append::find_absolute_path("C:\\OTHER\\README.TXT", out_path));
	EXPECT_EQ(out_path, "C:\\DIR\\README.TXT");

	// Disable path override
	dos_append::SetFlags(false, false, false);
	EXPECT_FALSE(dos_append::find_absolute_path("C:\\OTHER\\README.TXT", out_path));
}

// Requirement: Verify /PATH:ON resolves relative, absolute, dot-segment, and slash-mixed inputs. Target: dos_append::find_absolute_path()
TEST_F(DosAppendTest, PathOverrideVariants)
{
	// Setup target file in appended directory
	DOS_MakeDir("C:\\APPEND_LIB");
	uint16_t entry;
	DOS_CreateFile("C:\\APPEND_LIB\\TARGET.TXT", 0, &entry);
	DOS_CloseFile(entry);

	dos_append::SetDirectories("C:\\APPEND_LIB");
	dos_append::SetFlags(false, true, false); // Enable /PATH:ON

	std::string out_path;

	// 1. Relative path input
	EXPECT_TRUE(dos_append::find_absolute_path("..\\SUBDIR\\TARGET.TXT", out_path));
	EXPECT_EQ(out_path, "C:\\APPEND_LIB\\TARGET.TXT");

	// 2. Absolute path input
	EXPECT_TRUE(dos_append::find_absolute_path("D:\\OTHER\\DIR\\TARGET.TXT", out_path));
	EXPECT_EQ(out_path, "C:\\APPEND_LIB\\TARGET.TXT");

	// 3. Mixed slash styles
	EXPECT_TRUE(dos_append::find_absolute_path("D:/OTHER/DIR\\TARGET.TXT", out_path));
	EXPECT_EQ(out_path, "C:\\APPEND_LIB\\TARGET.TXT");

	// 4. Dot segment input
	EXPECT_TRUE(dos_append::find_absolute_path("C:\\DIR\\.\\..\\TARGET.TXT", out_path));
	EXPECT_EQ(out_path, "C:\\APPEND_LIB\\TARGET.TXT");
}

// ================================================================================
// BEHAVIORAL / INTEGRATION TESTS: APPEND Command Execution
// ================================================================================

// Requirement: Verify shell APPEND command correctly parses and sets multiple directories. Target: APPEND::Run(), dos_append::SetDirectories()
TEST_F(DosAppendTest, CommandSetDirectories)
{
	DOS_MakeDir("C:\\DATA");
	DOS_MakeDir("C:\\MORE");
	auto* cmd = new CommandLine("APPEND", "C:\\DATA;C:\\MORE");
	APPEND prog;
	prog.cmd = cmd;
	prog.Run();

	EXPECT_EQ(dos_append::GetDirectories(), "C:\\DATA;C:\\MORE");
}

// Requirement: Verify edge-case matrix (quotes, leading semicolons, invalid path atomicity). Target: APPEND::Run(), dos_append::ValidateDirectories()
TEST_F(DosAppendTest, QAComprehensiveTestMatrix)
{
	DOS_MakeDir("C:\\DATA");
	DOS_MakeDir("C:\\MORE");

	{
		auto* cmd = new CommandLine("APPEND", R"(  "C:\DATA"  ;  C:\MORE  )");
		APPEND prog;
		prog.cmd = cmd;
		prog.Run();
		EXPECT_EQ(dos_append::GetDirectories(), R"(C:\DATA;C:\MORE)");
	}

	{
		auto* cmd = new CommandLine("APPEND", R"(;C:\DATA;)");
		APPEND prog;
		prog.cmd = cmd;
		prog.Run();
		EXPECT_EQ(dos_append::GetDirectories(), R"(C:\DATA)");
	}

	{
		dos_append::SetDirectories(R"(C:\DATA)");
		auto* cmd = new CommandLine("APPEND", R"(C:\DATA;Z:\NONEXISTENT)");
		APPEND prog;
		prog.cmd = cmd;
		prog.Run();
		EXPECT_EQ(dos_append::GetDirectories(), R"(C:\DATA)");
	}
}

// ================================================================================
// BEHAVIORAL / INTEGRATION TESTS: DOS_OpenFile Hook
// ================================================================================

// Requirement: Verify DOS_OpenFile hooks search APPEND directories on file-not-found errors. Target: DOS_OpenFile(), dos_append::find_absolute_path()
TEST_F(DosAppendTest, HookCoreFeatureFlow)
{
	DOS_MakeDir("C:\\APPEND_DIR");
	uint16_t created_entry;
	DOS_CreateFile("C:\\APPEND_DIR\\README.TXT", 0, &created_entry);
	DOS_CloseFile(created_entry);

	uint16_t entry;
	dos.errorcode = 0;
	bool success  = DOS_OpenFile("README.TXT", OPEN_READ, &entry);
	EXPECT_FALSE(success);
	EXPECT_EQ(dos.errorcode, DOSERR_FILE_NOT_FOUND);

	dos_append::SetDirectories("C:\\OTHER_DIR");
	dos.errorcode = 0;
	success       = DOS_OpenFile("README.TXT", OPEN_READ, &entry);
	EXPECT_FALSE(success);
	EXPECT_EQ(dos.errorcode, DOSERR_FILE_NOT_FOUND);

	dos_append::SetDirectories("C:\\APPEND_DIR");
	dos.errorcode = 0;
	success       = DOS_OpenFile("README.TXT", OPEN_READ, &entry);
	EXPECT_TRUE(success);
	DOS_CloseFile(entry);

	dos.errorcode = 0;
	success       = DOS_OpenFile("MISSING.TXT", OPEN_READ, &entry);
	EXPECT_FALSE(success);
	EXPECT_EQ(dos.errorcode, DOSERR_FILE_NOT_FOUND);
}

// Requirement: Verify that file searching respects the multiplex interrupt B707h disable/enable state. Target: DOS_OpenFile(), dos_append::MultiplexHandler()
TEST_F(DosAppendTest, HookRespectsMultiplexDisable)
{
	DOS_MakeDir("C:\\APPEND_DIR");
	uint16_t created_entry;
	DOS_CreateFile("C:\\APPEND_DIR\\README.TXT", 0, &created_entry);
	DOS_CloseFile(created_entry);

	// Enable APPEND and verify we can open the file
	dos_append::SetDirectories("C:\\APPEND_DIR");
	uint16_t entry;
	EXPECT_TRUE(DOS_OpenFile("README.TXT", OPEN_READ, &entry));
	DOS_CloseFile(entry);

	// Call B707h with BX=0 (disable APPEND)
	reg_ah = 0xB7;
	reg_al = 0x07;
	reg_bx = 0x0000;
	EXPECT_TRUE(dos_append::MultiplexHandler());

	// Now trying to open the file should fail
	dos.errorcode = 0;
	EXPECT_FALSE(DOS_OpenFile("README.TXT", OPEN_READ, &entry));
	EXPECT_EQ(dos.errorcode, DOSERR_FILE_NOT_FOUND);

	// Call B707h with BX=1 (re-enable APPEND)
	reg_ah = 0xB7;
	reg_al = 0x07;
	reg_bx = 0x0001;
	EXPECT_TRUE(dos_append::MultiplexHandler());

	// Opening the file should succeed again
	EXPECT_TRUE(DOS_OpenFile("README.TXT", OPEN_READ, &entry));
	DOS_CloseFile(entry);
}


// ================================================================================
// BEHAVIORAL / INTEGRATION TESTS: Multiplex Handler
// ================================================================================

// Requirement: Verify INT 2Fh AH=B7h AL=00h installation check returns AL=FFh. Target: dos_append::MultiplexHandler()
TEST_F(DosAppendTest, MultiplexInstallationCheck)
{
	reg_ah       = 0xB7;
	reg_al       = 0x00;
	bool handled = dos_append::MultiplexHandler();
	EXPECT_TRUE(handled);
	EXPECT_EQ(reg_al, 0xFF);
}

// Requirement: Verify INT 2Fh AH=B7h AL=02h version check returns AX=FFFFh. Target: dos_append::MultiplexHandler()
TEST_F(DosAppendTest, MultiplexVersionCheck)
{
	reg_ah       = 0xB7;
	reg_al       = 0x02;
	bool handled = dos_append::MultiplexHandler();
	EXPECT_TRUE(handled);
	EXPECT_EQ(reg_ax, 0xFFFF);
}

// Requirement: Verify INT 2Fh AH=B7h AL=04h returns ES:DI pointer to DOS memory directory list. Target: dos_append::MultiplexHandler()
TEST_F(DosAppendTest, MultiplexDirPointer)
{
	dos_append::SetDirectories("C:\\GAMES;D:\\DATA");

	reg_ah       = 0xB7;
	reg_al       = 0x04;
	bool handled = dos_append::MultiplexHandler();
	EXPECT_TRUE(handled);

	EXPECT_NE(SegValue(es), 0);
	EXPECT_EQ(reg_di, 0x0000);

	PhysPt dos_addr = (static_cast<PhysPt>(SegValue(es)) << 4) + reg_di;
	std::string read_back;
	for (size_t i = 0; i < 256; ++i) {
		char c = static_cast<char>(
		        mem_readb(dos_addr + static_cast<PhysPt>(i)));
		if (c == '\0') {
			break;
		}
		read_back += c;
	}
	EXPECT_EQ(read_back, "C:\\GAMES;D:\\DATA");

	dos_append::SetDirectories("E:\\NEW");

	// Re-query ES:DI via INT 2Fh AL=04h API contract
	reg_ah  = 0xB7;
	reg_al  = 0x04;
	handled = dos_append::MultiplexHandler();
	EXPECT_TRUE(handled);

	dos_addr = (static_cast<PhysPt>(SegValue(es)) << 4) + reg_di;
	read_back.clear();
	for (size_t i = 0; i < 256; ++i) {
		char c = static_cast<char>(
		        mem_readb(dos_addr + static_cast<PhysPt>(i)));
		if (c == '\0') {
			break;
		}
		read_back += c;
	}
	EXPECT_EQ(read_back, "E:\\NEW");
}

// Requirement: Verify INT 2Fh AH=B7h AL=06h retrieves enabled state in BX. Target: dos_append::MultiplexHandler()
TEST_F(DosAppendTest, MultiplexGetState)
{
	reg_ah       = 0xB7;
	reg_al       = 0x06;
	bool handled = dos_append::MultiplexHandler();
	EXPECT_TRUE(handled);
	EXPECT_EQ(reg_bx, 0x0001);

	reg_ah = 0xB7;
	reg_al = 0x07;
	reg_bx = 0x0000;
	dos_append::MultiplexHandler();

	reg_ah  = 0xB7;
	reg_al  = 0x06;
	handled = dos_append::MultiplexHandler();
	EXPECT_TRUE(handled);
	EXPECT_EQ(reg_bx, 0x0000);

	reg_ah = 0xB7;
	reg_al = 0x07;
	reg_bx = 0x0001;
	dos_append::MultiplexHandler();
}

// Requirement: Verify INT 2Fh AH=B7h AL=07h sets/clears enabled state from BX. Target: dos_append::MultiplexHandler()
TEST_F(DosAppendTest, MultiplexSetState)
{
	dos_append::SetDirectories("C:\\DIR");
	EXPECT_TRUE(dos_append::IsEnabled());

	reg_ah       = 0xB7;
	reg_al       = 0x07;
	reg_bx       = 0x0000;
	bool handled = dos_append::MultiplexHandler();
	EXPECT_TRUE(handled);
	EXPECT_FALSE(dos_append::IsEnabled());

	reg_ah  = 0xB7;
	reg_al  = 0x07;
	reg_bx  = 0x0001;
	handled = dos_append::MultiplexHandler();
	EXPECT_TRUE(handled);
	EXPECT_TRUE(dos_append::IsEnabled());
}

// Requirement: Verify INT 2Fh AH=B7h AL=10h returns version, mode flags, and major/minor version. Target: dos_append::MultiplexHandler()
TEST_F(DosAppendTest, MultiplexDOSVersionCheck)
{
	dos_append::SetDirectories("C:\\DIR");

	reg_ah       = 0xB7;
	reg_al       = 0x10;
	bool handled = dos_append::MultiplexHandler();
	EXPECT_TRUE(handled);

	EXPECT_EQ(reg_ax, 0x0001);
	EXPECT_EQ(reg_bx, 0x0000);
	EXPECT_EQ(reg_cx, 0x0000);
	EXPECT_EQ(reg_dl, dos.version.major);
	EXPECT_EQ(reg_dh, dos.version.minor);
}

// Requirement: Verify unsupported INT 2Fh AH=B7h subfunctions return false. Target: dos_append::MultiplexHandler()
TEST_F(DosAppendTest, MultiplexIgnoredSubfunctions)
{
	reg_ah       = 0xB7;
	reg_al       = 0x05;
	bool handled = dos_append::MultiplexHandler();
	EXPECT_FALSE(handled);
}

// Requirement: Verify INT 2Fh AH=B7h AL=03h process sync returns true. Target: dos_append::MultiplexHandler()
TEST_F(DosAppendTest, MultiplexTopViewSync)
{
	reg_ah       = 0xB7;
	reg_al       = 0x03;
	bool handled = dos_append::MultiplexHandler();
	EXPECT_TRUE(handled);
}

// Requirement: Verify INT 2Fh AH=B7h AL=01h and 11h return true. Target: dos_append::MultiplexHandler()
TEST_F(DosAppendTest, MultiplexLegacyAndTrueNameSupported)
{
	reg_ah = 0xB7;
	reg_al = 0x01;
	EXPECT_TRUE(dos_append::MultiplexHandler());

	reg_ah = 0xB7;
	reg_al = 0x11;
	EXPECT_TRUE(dos_append::MultiplexHandler());
}

// ================================================================================
// UNIT TESTS: Path Length Limits and LFN Resolution
// ================================================================================

// // Requirement: Verify directory paths equal to or exceeding 80 characters abort validation. Target: APPEND::Run(), dos_append::ValidateDirectories()
TEST_F(DosAppendTest, ParserPathLengthLimit)
{
	DOS_MakeDir("C:\\GOOD");
	dos_append::SetDirectories("C:\\GOOD");

	// 1. Path exceeding DOS_PATHLENGTH limit (path part excluding drive must be < 80 characters)
	std::string path_80 = "C:\\";
	path_80 += std::string(80, 'B'); // 3 + 80 = 83 chars
	DOS_MakeDir(path_80.c_str());

	{
		auto* cmd = new CommandLine("APPEND", path_80);
		APPEND prog;
		prog.cmd = cmd;
		prog.Run();

		// Excessively long path input should abort and preserve previous directory list
		EXPECT_EQ(dos_append::GetDirectories(), "C:\\GOOD");
	}
}

// Requirement: Verify files inside long host directories are correctly resolved and opened via their short path aliases. Target: dos_append::ResolvePath(), DOS_OpenFile()
TEST_F(DosAppendTest, ResolvePathLongHostDir)
{
	// 1. Create a 76-character long host directory directly on the host filesystem
	std::string host_dir = "tests/files/append/";
	host_dir += std::string(76, 'A');
	std::filesystem::create_directories(host_dir);

	// 2. Create a file inside that directory using the host filesystem directly
	std::string host_file_path = host_dir + "/TESTFILE.TXT";
	FILE* f = fopen(host_file_path.c_str(), "w");
	ASSERT_NE(f, nullptr);
	fclose(f);

	// 3. Refresh the virtual drive cache so it notices the new host directory
	if (Drives[2]) {
		Drives[2]->EmptyCache();
	}

	// 4. Append the DOS-visible short name of the long directory
	auto* cmd = new CommandLine("APPEND", "C:\\AAAAAA~1");
	APPEND prog;
	prog.cmd = cmd;
	prog.Run();
	ASSERT_EQ(dos_append::GetDirectories(), "C:\\AAAAAA~1");

	// 5. Verify APPEND resolves the file name to the short path version
	std::string out_path;
	EXPECT_TRUE(dos_append::find_absolute_path("TESTFILE.TXT", out_path));
	EXPECT_EQ(out_path, "C:\\AAAAAA~1\\TESTFILE.TXT");

	// 6. Verify DOS can open the file using the resolved short path
	uint16_t open_handle;
	EXPECT_TRUE(DOS_OpenFile(out_path.c_str(), OPEN_READ, &open_handle));
	DOS_CloseFile(open_handle);

	// 7. Clean up host filesystem test directory
	std::filesystem::remove_all(host_dir);
	if (Drives[2]) {
		Drives[2]->EmptyCache();
	}
}

// ================================================================================
// INTEGRATION TESTS: /X Execution Search
// ================================================================================

// Requirement: Verify /X:ON enables wildcard directory searching in DOS_FindFirst. Target: DOS_FindFirst(), dos_append::IsExecOn()
TEST_F(DosAppendTest, FindFirstWildcardResolution)
{
	dos_append::SetDirectories("C:\\DIR");

	// Without /X:ON, FindFirst should fail for APPEND directories
	dos_append::SetFlags(false, true, false);
	EXPECT_FALSE(DOS_FindFirst("*.TXT", FatAttributeFlags::NotVolume));

	// With /X:ON, FindFirst should resolve *.TXT in C:\\DIR (finding README.TXT)
	dos_append::SetFlags(false, true, true);
	EXPECT_TRUE(DOS_FindFirst("*.TXT", FatAttributeFlags::NotVolume));

	DOS_DTA dta(dos.dta());
	DOS_DTA::Result res = {};
	dta.GetResult(res);
	EXPECT_EQ(res.name, "README.TXT");

	// Negative wildcard tests: Ensure wildcards match specifically and don't match everything
	EXPECT_TRUE(DOS_FindFirst("README.*", FatAttributeFlags::NotVolume));
	EXPECT_FALSE(DOS_FindFirst("*.EXE", FatAttributeFlags::NotVolume));
	EXPECT_FALSE(DOS_FindFirst("A*.TXT", FatAttributeFlags::NotVolume));
}

// Requirement: Verify /X:ON enables executable path lookup for files in APPEND directories. Target: dos_append::IsExecOn(), dos_append::find_absolute_path()
TEST_F(DosAppendTest, ExecModeExecutionSearch)
{
	DOS_MakeDir("C:\\BIN");
	uint16_t entry;
	DOS_CreateFile("C:\\BIN\\RUN.EXE", 0, &entry);
	DOS_CloseFile(entry);

	dos_append::SetDirectories("C:\\BIN");

	std::string resolved_path;

	// /X:OFF (default): Search for executable in APPEND path should be skipped / fail
	dos_append::SetFlags(false, true, false);
	EXPECT_FALSE(dos_append::IsExecOn());
	EXPECT_FALSE(dos_append::find_absolute_path("RUN.EXE", resolved_path));

	// /X:ON: Executable search succeeds and resolves RUN.EXE from C:\BIN
	dos_append::SetFlags(false, true, true);
	EXPECT_TRUE(dos_append::IsExecOn());
	EXPECT_TRUE(dos_append::find_absolute_path("RUN.EXE", resolved_path));
	EXPECT_EQ(resolved_path, "C:\\BIN\\RUN.EXE");
}

// ================================================================================
// INTEGRATION TESTS: /E Environment Variable Synchronization
// ================================================================================

// Requirement: Verify /E mode discovers external environment variable edits dynamically. Target: dos_append::GetDirectories(), IsEnvOn()
TEST_F(DosAppendTest, EnvModeExternalSetAppend)
{
	dos_append::SetFlags(true, true, false);
	dos_append::SetDirectories("C:\\DIR");

	// Simulate user/batch file modifying the shell environment directly
	if (auto shell = DOS_GetFirstShell()) {
		shell->SetEnv("APPEND", "C:\\DIR;C:\\OTHER");
	} else if (dos.psp() != 0) {
		DOS_PSP(dos.psp()).SetEnvironmentValue("APPEND", "C:\\DIR;C:\\OTHER");
	}

	// Verify GetDirectories() discovers the external environment change
	EXPECT_EQ(dos_append::GetDirectories(), "C:\\DIR;C:\\OTHER");

	// Invoke B704h and verify the returned DOS-memory string reflects the change on demand
	reg_ah       = 0xB7;
	reg_al       = 0x04;
	bool handled = dos_append::MultiplexHandler();
	EXPECT_TRUE(handled);

	char buf[128] = {};
	MEM_StrCopy(SegPhys(es) + reg_di, buf, sizeof(buf));
	EXPECT_EQ(std::string(buf), "C:\\DIR;C:\\OTHER");
}

// Requirement: Verify /E mode synchronizes directory list changes and clears cleanly. Target: dos_append::SetDirectories(), IsEnvOn()
TEST_F(DosAppendTest, EnvModeSyncAndClear)
{
	dos_append::SetFlags(true, true, false);
	dos_append::SetDirectories("C:\\DIR");
	EXPECT_EQ(dos_append::GetDirectories(), "C:\\DIR");

	reg_ah       = 0xB7;
	reg_al       = 0x04;
	bool handled = dos_append::MultiplexHandler();
	EXPECT_TRUE(handled);

	char buf[128] = {};
	MEM_StrCopy(SegPhys(es) + reg_di, buf, sizeof(buf));
	EXPECT_EQ(std::string(buf), "C:\\DIR");

	dos_append::SetDirectories("");
	EXPECT_EQ(dos_append::GetDirectories(), "");
	EXPECT_FALSE(dos_append::IsEnabled());
}

// Requirement: Verify B707h disable state overrides active directory list in /E mode. Target: dos_append::MultiplexHandler(), IsEnabled()
TEST_F(DosAppendTest, B707hDisableOverride)
{
	dos_append::SetFlags(true, true, false);
	dos_append::SetDirectories("C:\\DIR");
	EXPECT_TRUE(dos_append::IsEnabled());

	reg_ah       = 0xB7;
	reg_al       = 0x07;
	reg_bx       = 0x0000;
	bool handled = dos_append::MultiplexHandler();
	EXPECT_TRUE(handled);
	EXPECT_FALSE(dos_append::IsEnabled());

	reg_bx = 0x0001;
	dos_append::MultiplexHandler();
	EXPECT_TRUE(dos_append::IsEnabled());
}

// ================================================================================
// UNIT TESTS: Option Permutations and Illegal Switch Handling
// ================================================================================

// Requirement: Verify relative option ordering of orthogonal switches has no effect on final state. Target: APPEND::Run(), parse_options()
TEST_F(DosAppendTest, ParserFlagsPermutations)
{
	DOS_MakeDir("C:\\DATA");

	const std::vector<std::string> permutations = {
		"/X:ON /PATH:OFF /E C:\\DATA",
		"/X:ON /E /PATH:OFF C:\\DATA",
		"/PATH:OFF /X:ON /E C:\\DATA",
		"/PATH:OFF /E /X:ON C:\\DATA",
		"/E /X:ON /PATH:OFF C:\\DATA",
		"/E /PATH:OFF /X:ON C:\\DATA"
	};

	for (const auto& cmdline : permutations) {
		// Reset state before running each permutation
		dos_append::SetDirectories("");
		dos_append::SetFlags(false, true, false);

		auto* cmd = new CommandLine("APPEND", cmdline);
		APPEND prog;
		prog.cmd = cmd;
		prog.Run();

		EXPECT_TRUE(dos_append::IsExecOn());
		EXPECT_FALSE(dos_append::IsPathOverrideOn());
		EXPECT_TRUE(dos_append::IsEnvOn());
		EXPECT_EQ(dos_append::GetDirectories(), "C:\\DATA");
	}
}

// Requirement: Verify illegal/unrecognized switch inputs abort command execution without side-effects. Target: APPEND::Run(), parse_options()
TEST_F(DosAppendTest, ParserIllegalSwitchAborts)
{
	// Setup initial good state
	DOS_MakeDir("C:\\GOOD");
	DOS_MakeDir("C:\\DATA");
	dos_append::SetDirectories("C:\\GOOD");
	dos_append::SetFlags(false, true, false);

	// Case 1: Purely invalid switch value
	{
		auto* cmd = new CommandLine("APPEND", "/X:INVALID C:\\DATA");
		APPEND prog;
		prog.cmd = cmd;
		prog.Run();

		// Should NOT change directories or exec option
		EXPECT_EQ(dos_append::GetDirectories(), "C:\\GOOD");
		EXPECT_FALSE(dos_append::IsExecOn());
	}

	// Case 2: Mixed valid switch and invalid switch
	{
		auto* cmd = new CommandLine("APPEND", "/X:ON /INVALID C:\\DATA");
		APPEND prog;
		prog.cmd = cmd;
		prog.Run();

		// Should NOT apply /X:ON and should NOT update directories
		EXPECT_EQ(dos_append::GetDirectories(), "C:\\GOOD");
		EXPECT_FALSE(dos_append::IsExecOn());
	}

	// Case 3: Conflicting format switch
	{
		auto* cmd = new CommandLine("APPEND", "/X:ON:OFF C:\\DATA");
		APPEND prog;
		prog.cmd = cmd;
		prog.Run();

		EXPECT_EQ(dos_append::GetDirectories(), "C:\\GOOD");
		EXPECT_FALSE(dos_append::IsExecOn());
	}
}

} // namespace
