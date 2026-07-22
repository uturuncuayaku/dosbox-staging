// SPDX-FileCopyrightText:  2026 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#include "dos/dos_append.h"

#include <string>

#include <gtest/gtest.h>

#include "cpu/registers.h"
#include "dos/dos.h"
#include "dos/programs.h"
#include "dos/programs/append.h"
#include "shell/command_line.h"
#include "utils/string_utils.h"

#include "dos/dos_system.h"
#include "dos/drives.h"
#include "dosbox_test_fixture.h"

namespace {

class DOS_AppendTest : public DOSBoxTestFixture {
protected:
	void SetUp() override
	{
		DOSBoxTestFixture::SetUp();
		
		// Mount C drive to current directory so DOS_MakeDir works
		Drives[2] = std::make_shared<localDrive>(".", 512, 1, 1, 1, 1, false);
		DOS_SetDefaultDrive(2);

		// Ensure APPEND is clear before each test
		dos_append::SetDirList("");
	}

	void TearDown() override
	{
		Drives[2].reset();
		dos_append::SetDirList("");
		DOSBoxTestFixture::TearDown();
	}
};

// ================================================================================
// UNIT TESTS: State Management
// ================================================================================

TEST_F(DOS_AppendTest, Initialization)
{
	EXPECT_FALSE(dos_append::IsEnabled());
	EXPECT_EQ(dos_append::GetDirList(), "");
}

TEST_F(DOS_AppendTest, Activation)
{
	dos_append::SetDirList("C:\\DIR");
	EXPECT_TRUE(dos_append::IsEnabled());
}

TEST_F(DOS_AppendTest, Deactivation)
{
	dos_append::SetDirList("C:\\DIR");
	EXPECT_TRUE(dos_append::IsEnabled());

	dos_append::SetDirList("");
	EXPECT_FALSE(dos_append::IsEnabled());
	EXPECT_EQ(dos_append::GetDirList(), "");
}

TEST_F(DOS_AppendTest, ReplacementBehavior)
{
	dos_append::SetDirList("C:\\ONE");
	EXPECT_EQ(dos_append::GetDirList(), "C:\\ONE");

	// Consecutive calls completely replace the list
	dos_append::SetDirList("C:\\TWO");
	EXPECT_EQ(dos_append::GetDirList(), "C:\\TWO");
}

// ================================================================================
// UNIT TESTS: Parser and Normalization
// ================================================================================

TEST_F(DOS_AppendTest, ParserTrailingSeparators)
{
	// Test setting with trailing separators
	dos_append::SetDirList("C:\\DIR\\");

	// Since SetDirList stores the raw string, the normalization happens in ResolveName.
	// But let's verify how it resolves. We mock a file creation to test resolution.
	// We'll test this behavior properly in the ResolveName tests where it is applied.
	// However, we verify it is stored exactly as given.
	EXPECT_EQ(dos_append::GetDirList(), "C:\\DIR\\");
}

TEST_F(DOS_AppendTest, ParserEmptyClear)
{
	dos_append::SetDirList(";");
	EXPECT_EQ(dos_append::GetDirList(), ";");
}

TEST_F(DOS_AppendTest, ParserDuplicates)
{
	// Current implementation retains duplicates.
	dos_append::SetDirList("C:\\ONE;C:\\ONE;D:\\TWO");
	EXPECT_EQ(dos_append::GetDirList(), "C:\\ONE;C:\\ONE;D:\\TWO");
}

// ================================================================================
// UNIT TESTS: Path Resolution (ResolveName)
// ================================================================================

TEST_F(DOS_AppendTest, ResolveNameDisabledState)
{
	dos_append::SetDirList("");
	std::string out_path;
	EXPECT_FALSE(dos_append::ResolveName("FILE.TXT", out_path));
}

TEST_F(DOS_AppendTest, ResolveNameBasenameExtraction)
{
	dos_append::SetDirList("C:\\DIR");
	
	// Create a dummy file in the append dir
	DOS_MakeDir("C:\\DIR");
	uint16_t entry;
	DOS_CreateFile("C:\\DIR\\README.TXT", 0, &entry);
	DOS_CloseFile(entry);

	std::string out_path;

	// Extraction test: Absolute paths or paths with directories should bypass APPEND
	EXPECT_FALSE(dos_append::ResolveName("D:\\OTHER\\README.TXT", out_path));

	// Pure filenames should be resolved
	EXPECT_TRUE(dos_append::ResolveName("README.TXT", out_path));
	EXPECT_EQ(out_path, "C:\\DIR\\README.TXT");
}

TEST_F(DOS_AppendTest, ResolveNameOrderingAndNormalization)
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

	// Case sensitivity check for dir storage and trailing separator normalization
	// Note: DOS paths are case-insensitive.
	dos_append::SetDirList("c:\\two\\;;;C:\\ONE\\;C:\\THREE");

	std::string out_path;
	EXPECT_TRUE(dos_append::ResolveName("FILE.TXT", out_path));
	// Should pick C:\ONE over C:\THREE due to ordering
	EXPECT_EQ(out_path, "C:\\ONE\\FILE.TXT");
}

// ================================================================================
// BEHAVIORAL / INTEGRATION TESTS: APPEND Command Execution
// ================================================================================

// We don't need a MockAPPEND class because we don't strictly test the string output, 
// only the state side effects. APPEND is marked final anyway.

TEST_F(DOS_AppendTest, CommandSetDirectories)
{
	auto* cmd = new CommandLine("APPEND", "C:\\DATA;D:\\MORE");
	APPEND prog;
	prog.cmd = cmd;
	prog.Run();

	EXPECT_EQ(dos_append::GetDirList(), "C:\\DATA;D:\\MORE");
}

TEST_F(DOS_AppendTest, CommandClearDirectories)
{
	dos_append::SetDirList("C:\\DATA");

	auto* cmd = new CommandLine("APPEND", ";");
	APPEND prog;
	prog.cmd = cmd;
	prog.Run();

	EXPECT_EQ(dos_append::GetDirList(), "");
	EXPECT_FALSE(dos_append::IsEnabled());
}

// ================================================================================
// BEHAVIORAL / INTEGRATION TESTS: DOS_OpenFile Hook
// ================================================================================

TEST_F(DOS_AppendTest, HookCoreFeatureFlow)
{
	// Setup
	DOS_MakeDir("C:\\APPEND_DIR");
	uint16_t created_entry;
	DOS_CreateFile("C:\\APPEND_DIR\\README.TXT", 0, &created_entry);
	DOS_CloseFile(created_entry);

	// 1. Inactive: should fail with FILE_NOT_FOUND
	uint16_t entry;
	dos.errorcode = 0;
	bool success = DOS_OpenFile("README.TXT", OPEN_READ, &entry);
	EXPECT_FALSE(success);
	EXPECT_EQ(dos.errorcode, DOSERR_FILE_NOT_FOUND);

	// 2. Active, file not in list: should fail with original error
	dos_append::SetDirList("C:\\OTHER_DIR");
	dos.errorcode = 0;
	success = DOS_OpenFile("README.TXT", OPEN_READ, &entry);
	EXPECT_FALSE(success);
	EXPECT_EQ(dos.errorcode, DOSERR_FILE_NOT_FOUND); // original error in current dir was FILE_NOT_FOUND

	// 3. Active, file in list: should succeed
	dos_append::SetDirList("C:\\APPEND_DIR");
	dos.errorcode = 0;
	success = DOS_OpenFile("README.TXT", OPEN_READ, &entry);
	EXPECT_TRUE(success);
	DOS_CloseFile(entry);

	// 4. Error Preservation
	dos.errorcode = 0;
	success = DOS_OpenFile("MISSING.TXT", OPEN_READ, &entry);
	EXPECT_FALSE(success);
	EXPECT_EQ(dos.errorcode, DOSERR_FILE_NOT_FOUND); // Original error code is preserved
}

TEST_F(DOS_AppendTest, HookNegativeTestAbsolutePaths)
{
	DOS_MakeDir("C:\\APPEND_DIR");
	uint16_t created_entry;
	DOS_CreateFile("C:\\APPEND_DIR\\FILE.TXT", 0, &created_entry);
	DOS_CloseFile(created_entry);

	dos_append::SetDirList("C:\\APPEND_DIR");

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

TEST_F(DOS_AppendTest, MultiplexInstallationCheck)
{
	reg_ah = 0xB7;
	reg_al = 0x00;
	bool handled = dos_append::MultiplexHandler();
	EXPECT_TRUE(handled);
	EXPECT_EQ(reg_al, 0xFF);
}

TEST_F(DOS_AppendTest, MultiplexIgnoredSubfunctions)
{
	// Version Check & Ignored Subfunctions
	reg_ah = 0xB7;
	reg_al = 0x01; // Version check
	bool handled = dos_append::MultiplexHandler();
	EXPECT_FALSE(handled);

	reg_ah = 0xB7;
	reg_al = 0x11; // Other subfunction
	handled = dos_append::MultiplexHandler();
	EXPECT_FALSE(handled);
}

} // namespace
