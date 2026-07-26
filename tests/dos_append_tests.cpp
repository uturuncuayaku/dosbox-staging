// SPDX-FileCopyrightText:  2026 Antigravity
// SPDX-FileCopyrightText:  2026 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#include "dos/dos_append.h"

#include <string>

#include <gtest/gtest.h>

#include "cpu/registers.h"
#include "dos/dos.h"
#include "dos/programs/append.h"
#include "shell/command_line.h"

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

		LOG_MSG("Mount C drive to current directory so DOS_MakeDir works");
		Drives[2] = std::make_shared<localDrive>(".", 512, 1, 1, 1, 1, false);
		DOS_SetDefaultDrive(2);

		LOG_MSG("Ensure APPEND is clear before each test");
		dos_append::SetDirectories("");
	}

	void TearDown() override
	{
		LOG_MSG("Tearing down the test environment");
		LOG_MSG("Resetting the drives list");
		Drives[2].reset();
		dos_append::SetDirectories("");
		LOG_MSG("Resetting the dos test fixture");
		DOSBoxTestFixture::TearDown();
	}
};

// ================================================================================
// UNIT TESTS: State Management
// ================================================================================

TEST_F(DosAppendTest, Initialization)
{
	EXPECT_FALSE(dos_append::IsEnabled());
	EXPECT_EQ(dos_append::GetDirectories(), "");
}

TEST_F(DosAppendTest, Activation)
{
	dos_append::SetDirectories("C:\\DIR");
	EXPECT_TRUE(dos_append::IsEnabled());
}

TEST_F(DosAppendTest, Deactivation)
{
	dos_append::SetDirectories("C:\\DIR");
	EXPECT_TRUE(dos_append::IsEnabled());

	dos_append::SetDirectories("");
	EXPECT_FALSE(dos_append::IsEnabled());
	EXPECT_EQ(dos_append::GetDirectories(), "");
}

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

TEST_F(DosAppendTest, ParserTrailingSeparators)
{
	// Test setting with trailing separators
	dos_append::SetDirectories("C:\\DIR\\");

	// Since SetDirectories stores the raw string, the normalization happens in
	// ResolveName. But let's verify how it resolves. We mock a file
	// creation to test resolution. We'll test this behavior properly in the
	// ResolveName tests where it is applied. However, we verify it is
	// stored exactly as given.
	EXPECT_EQ(dos_append::GetDirectories(), "C:\\DIR\\");
}

TEST_F(DosAppendTest, ParserEmptyClear)
{
	dos_append::SetDirectories(";");
	EXPECT_EQ(dos_append::GetDirectories(), ";");
}

TEST_F(DosAppendTest, ParserDuplicates)
{
	// Current implementation retains duplicates.
	DOS_MakeDir("C:\\ONE");
	DOS_MakeDir("C:\\TWO");
	auto* cmd = new CommandLine("APPEND", "C:\\ONE;C:\\ONE;C:\\TWO");
	APPEND prog;
	prog.cmd = cmd;
	prog.Run();
	EXPECT_EQ(dos_append::GetDirectories(), "C:\\ONE;C:\\ONE;C:\\TWO");
}

TEST_F(DosAppendTest, ParserSwitches)
{
	DOS_MakeDir("C:\\DIR");
	auto* cmd = new CommandLine("APPEND", "/X:ON /PATH:OFF C:\\DIR /X");
	APPEND prog;
	prog.cmd = cmd;
	prog.Run();
	EXPECT_EQ(dos_append::GetDirectories(), "C:\\DIR");
}

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

TEST_F(DosAppendTest, ParserAbsoluteExpansion)
{
	DOS_MakeDir("C:\\TEST");
	DOS_MakeDir("C:\\TEST\\DATA");

	// Set DOS CWD to C:\TEST
	DOS_SetDefaultDrive(2);
	DOS_ChangeDir("TEST");

	auto* cmd = new CommandLine("APPEND", "DATA");
	APPEND prog;
	prog.cmd = cmd;
	prog.Run();

	EXPECT_EQ(dos_append::GetDirectories(), "C:\\TEST\\DATA");

	// Reset DOS CWD back to root for other tests
	DOS_ChangeDir("\\");
}

TEST_F(DosAppendTest, ParserInvalidPath)
{
	dos_append::SetDirectories("C:\\GOOD");

	auto* cmd = new CommandLine("APPEND", "C:\\NONEXISTENT");
	APPEND prog;
	prog.cmd = cmd;
	prog.Run();

	// Because C:\NONEXISTENT is invalid, APPEND should abort
	// and the original directory list should NOT be modified.
	EXPECT_EQ(dos_append::GetDirectories(), "C:\\GOOD");
}

TEST_F(DosAppendTest, ParserEqualSign)
{
	DOS_MakeDir("C:\\DATA");
	auto* cmd = new CommandLine("APPEND", "=C:\\DATA");
	APPEND prog;
	prog.cmd = cmd;
	prog.Run();

	EXPECT_EQ(dos_append::GetDirectories(), "C:\\DATA");
}

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

// ================================================================================
// UNIT TESTS: Path Resolution (ResolveName)
// ================================================================================

TEST_F(DosAppendTest, ResolveNameDisabledState)
{
	dos_append::SetDirectories("");
	std::string out_path;
	EXPECT_FALSE(dos_append::find_absolute_path("FILE.TXT", out_path));
}

TEST_F(DosAppendTest, ResolveNameBasenameExtraction)
{
	dos_append::SetDirectories("C:\\DIR");

	// Create a dummy file in the append dir
	DOS_MakeDir("C:\\DIR");
	uint16_t entry;
	DOS_CreateFile("C:\\DIR\\README.TXT", 0, &entry);
	DOS_CloseFile(entry);

	std::string out_path;

	// Extraction test: Absolute paths or paths with directories should
	// bypass APPEND
	EXPECT_FALSE(dos_append::find_absolute_path("D:\\OTHER\\README.TXT", out_path));

	// Pure filenames should be resolved
	EXPECT_TRUE(dos_append::find_absolute_path("README.TXT", out_path));
	EXPECT_EQ(out_path, "C:\\DIR\\README.TXT");
}

TEST_F(DosAppendTest, ResolveNameOrderingAndNormalization)
{
	// Create directories
	DOS_MakeDir("C:\\ONE");
	DOS_MakeDir("C:\\TWO");
	DOS_MakeDir("C:\\THREE");

	// Create test file in multiple directories
	uint16_t entry;
	DOS_CreateFile("C:\\ONE\\FILE.TXT", 0, &entry);
	DOS_CloseFile(entry);
	DOS_CreateFile("C:\\THREE\\FILE.TXT", 0, &entry);
	DOS_CloseFile(entry);

	// Case sensitivity check for dir storage and trailing separator
	// normalization Note: DOS paths are case-insensitive.
	dos_append::SetDirectories("c:\\two\\;;;C:\\ONE\\;C:\\THREE");

	std::string out_path;
	EXPECT_TRUE(dos_append::find_absolute_path("FILE.TXT", out_path));
	// Should pick C:\ONE over C:\THREE due to ordering
	EXPECT_EQ(out_path, "C:\\ONE\\FILE.TXT");
}

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

TEST_F(DosAppendTest, PathOverrideFuzzingGeometries)
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

// We don't need a MockAPPEND class because we don't strictly test the string
// output, only the state side effects. APPEND is marked final anyway.

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

TEST_F(DosAppendTest, QAComprehensiveTestMatrix)
{
	DOS_MakeDir("C:\\DATA");
	DOS_MakeDir("C:\\MORE");

	// 1. Whitespace & Quote Trimming (using modern C++ raw string literals R"(...)")
	{
		auto* cmd = new CommandLine("APPEND", R"(  "C:\DATA"  ;  C:\MORE  )");
		APPEND prog;
		prog.cmd = cmd;
		prog.Run();
		EXPECT_EQ(dos_append::GetDirectories(), R"(C:\DATA;C:\MORE)");
	}

	// 2. Trailing and Leading Semicolons
	{
		auto* cmd = new CommandLine("APPEND", R"(;C:\DATA;)");
		APPEND prog;
		prog.cmd = cmd;
		prog.Run();
		EXPECT_EQ(dos_append::GetDirectories(), R"(C:\DATA)");
	}

	// 3. Invalid Path Abort (Atomicity Check)
	{
		dos_append::SetDirectories(R"(C:\DATA)");
		auto* cmd = new CommandLine("APPEND", R"(C:\DATA;Z:\NONEXISTENT)");
		APPEND prog;
		prog.cmd = cmd;
		prog.Run();
		// Should fail validation and keep original dir list intact
		EXPECT_EQ(dos_append::GetDirectories(), R"(C:\DATA)");
	}
}

// ================================================================================
// BEHAVIORAL / INTEGRATION TESTS: DOS_OpenFile Hook
// ================================================================================

TEST_F(DosAppendTest, HookCoreFeatureFlow)
{
	// Setup
	DOS_MakeDir("C:\\APPEND_DIR");
	uint16_t created_entry;
	DOS_CreateFile("C:\\APPEND_DIR\\README.TXT", 0, &created_entry);
	DOS_CloseFile(created_entry);

	// 1. Inactive: should fail with FILE_NOT_FOUND
	uint16_t entry;
	dos.errorcode = 0;
	bool success  = DOS_OpenFile("README.TXT", OPEN_READ, &entry);
	EXPECT_FALSE(success);
	EXPECT_EQ(dos.errorcode, DOSERR_FILE_NOT_FOUND);

	// 2. Active, file not in list: should fail with original error
	dos_append::SetDirectories("C:\\OTHER_DIR");
	dos.errorcode = 0;
	success       = DOS_OpenFile("README.TXT", OPEN_READ, &entry);
	EXPECT_FALSE(success);
	EXPECT_EQ(dos.errorcode, DOSERR_FILE_NOT_FOUND); // original error in
	                                                 // current dir was
	                                                 // FILE_NOT_FOUND

	// 3. Active, file in list: should succeed
	dos_append::SetDirectories("C:\\APPEND_DIR");
	dos.errorcode = 0;
	success       = DOS_OpenFile("README.TXT", OPEN_READ, &entry);
	EXPECT_TRUE(success);
	DOS_CloseFile(entry);

	// 4. Error Preservation
	dos.errorcode = 0;
	success       = DOS_OpenFile("MISSING.TXT", OPEN_READ, &entry);
	EXPECT_FALSE(success);
	EXPECT_EQ(dos.errorcode, DOSERR_FILE_NOT_FOUND); // Original error code
	                                                 // is preserved
}

TEST_F(DosAppendTest, HookNegativeTestAbsolutePaths)
{
	DOS_MakeDir("C:\\APPEND_DIR");
	uint16_t created_entry;
	DOS_CreateFile("C:\\APPEND_DIR\\FILE.TXT", 0, &created_entry);
	DOS_CloseFile(created_entry);

	dos_append::SetDirectories("C:\\APPEND_DIR");
	dos_append::SetFlags(false, false, false); // /PATH:OFF: skip APPEND search for paths with drive/backslash

	uint16_t entry;

	// Paths with drive letters should NOT trigger APPEND
	bool success = DOS_OpenFile("A:FILE.TXT", OPEN_READ, &entry);
	EXPECT_FALSE(success);

	// Paths with directory separators should NOT trigger APPEND
	success = DOS_OpenFile("SUB\\FILE.TXT", OPEN_READ, &entry);
	EXPECT_FALSE(success);

	success = DOS_OpenFile("C:\\OTHER\\FILE.TXT", OPEN_READ, &entry);
	EXPECT_FALSE(success);
}

// ================================================================================
// BEHAVIORAL / INTEGRATION TESTS: Multiplex Handler
// ================================================================================

TEST_F(DosAppendTest, MultiplexInstallationCheck)
{
	reg_ah       = 0xB7;
	reg_al       = 0x00;
	bool handled = dos_append::MultiplexHandler();
	EXPECT_TRUE(handled);
	EXPECT_EQ(reg_al, 0xFF);
}

TEST_F(DosAppendTest, MultiplexVersionCheck)
{
	// Legacy version check (02h): MS-DOS APPEND.ASM returns AX=FFFFh
	// to signal "I am MS-DOS APPEND, not IBM PC Network APPEND."
	reg_ah       = 0xB7;
	reg_al       = 0x02;
	bool handled = dos_append::MultiplexHandler();
	EXPECT_TRUE(handled);
	EXPECT_EQ(reg_ax, 0xFFFF);
}
TEST_F(DosAppendTest, MultiplexDirPointer)
{
	// Dir pointer (04h): returns ES:DI pointing to the directory list
	// in emulated DOS memory. The string should match our C++ dir_list.

	dos_append::SetDirectories("C:\\GAMES;D:\\DATA");

	reg_ah       = 0xB7;
	reg_al       = 0x04;
	bool handled = dos_append::MultiplexHandler();
	EXPECT_TRUE(handled);

	// ES:DI should point to a valid segment with offset 0
	EXPECT_NE(SegValue(es), 0);
	EXPECT_EQ(reg_di, 0x0000);

	// Read the string back from emulated DOS memory and verify it matches
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

	// Verify sync: update the list and re-read
	dos_append::SetDirectories("E:\\NEW");
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

TEST_F(DosAppendTest, MultiplexGetState)
{
	// Get state (06h): returns mode_flags in BX

	// When disabled, BX should be 0
	dos_append::SetDirectories("");
	reg_ah       = 0xB7;
	reg_al       = 0x06;
	bool handled = dos_append::MultiplexHandler();
	EXPECT_TRUE(handled);
	EXPECT_EQ(reg_bx, 0x0000);

	// When enabled, BX should have the Enabled bit set
	dos_append::SetDirectories("C:\\DIR");
	reg_ah  = 0xB7;
	reg_al  = 0x06;
	handled = dos_append::MultiplexHandler();
	EXPECT_TRUE(handled);
	EXPECT_EQ(reg_bx, 0x0001);
}

TEST_F(DosAppendTest, MultiplexSetState)
{
	// Set state (07h): accepts BX and honors the Enabled bit

	// Start with APPEND enabled
	dos_append::SetDirectories("C:\\DIR");
	EXPECT_TRUE(dos_append::IsEnabled());

	// Disable via set_state by clearing the Enabled bit
	reg_ah       = 0xB7;
	reg_al       = 0x07;
	reg_bx       = 0x0000;
	bool handled = dos_append::MultiplexHandler();
	EXPECT_TRUE(handled);
	EXPECT_FALSE(dos_append::IsEnabled());

	// Re-enable via set_state (dir_list is still non-empty)
	reg_ah  = 0xB7;
	reg_al  = 0x07;
	reg_bx  = 0x0001;
	handled = dos_append::MultiplexHandler();
	EXPECT_TRUE(handled);
	EXPECT_TRUE(dos_append::IsEnabled());
}

TEST_F(DosAppendTest, MultiplexDOSVersionCheck)
{
	// Detailed DOS version check (10h):
	// Returns AX=mode_flags, BX=0, CX=0, DL=major, DH=minor.
	dos_append::SetDirectories("C:\\DIR");

	reg_ah       = 0xB7;
	reg_al       = 0x10;
	bool handled = dos_append::MultiplexHandler();
	EXPECT_TRUE(handled);

	// AX should contain mode_flags (Enabled = 0x0001)
	EXPECT_EQ(reg_ax, 0x0001);
	// BX and CX cleared
	EXPECT_EQ(reg_bx, 0x0000);
	EXPECT_EQ(reg_cx, 0x0000);
	// DL=major, DH=minor from dos.version
	EXPECT_EQ(reg_dl, dos.version.major);
	EXPECT_EQ(reg_dh, dos.version.minor);
}

TEST_F(DosAppendTest, MultiplexIgnoredSubfunctions)
{
	// Subfunction 01h (not supported / ignored)
	reg_ah       = 0xB7;
	reg_al       = 0x01;
	bool handled = dos_append::MultiplexHandler();
	EXPECT_FALSE(handled);

	// Subfunction 11h (true_name, not implemented)
	reg_ah  = 0xB7;
	reg_al  = 0x11;
	handled = dos_append::MultiplexHandler();
	EXPECT_FALSE(handled);
}

} // namespace
