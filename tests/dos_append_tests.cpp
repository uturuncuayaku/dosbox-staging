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
	dos_append::SetDirectories(";");
	EXPECT_EQ(dos_append::GetDirectories(), ";");
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

} // namespace
