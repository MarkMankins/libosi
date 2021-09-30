#include <offset/i_t.h>
#include <offset/offset.h>

#include "wintrospection/wintrospection.h"
#include "gtest/gtest.h"
#include <iohal/memory/virtual_memory.h>
#include <set>
#include <unistd.h>

#include <iostream>
#include <map>

#include "wintrospection/utils.h"

char* testfile = nullptr;

TEST(TestAmd64CallTracer, Win7SP1Amd64)
{
    ASSERT_TRUE(testfile) << "Couldn't load input test file!";
    ASSERT_TRUE(access(testfile, R_OK) == 0) << "Could not read input file";

    struct WindowsKernelDetails kdetails = {0};
    struct WindowsKernelOSI s_kosi = {0};
    kdetails.pointer_width = 8;
    kdetails.kpcr = 0xfffff80002834d00;
    // kdetails.kdbg = 0xfffff8000284b0a0;
    pm_addr_t asid = 0x335ef000;
    bool pae = false;

    s_kosi.pmem = load_physical_memory_snapshot(testfile);
    s_kosi.kernel_tlib = load_type_library("windows-64-7sp1");
    ASSERT_TRUE(s_kosi.pmem != nullptr) << "failed to load physical memory snapshot";
    ASSERT_TRUE(s_kosi.kernel_tlib != nullptr) << "failed to load type library";
    ASSERT_TRUE(initialize_windows_kernel_osi(&s_kosi, &kdetails, asid, pae))
        << "Failed to initialize kernel osi";

    struct WindowsKernelOSI* kosi = &s_kosi;

    auto proc = kosi_get_current_process(kosi);
    WindowsProcessOSI posi;
    ASSERT_TRUE(init_process_osi_from_pid(kosi, &posi, process_get_pid(proc)));
    osi::i_t eproc =
        osi::i_t(posi.vmem, kosi->kernel_tlib, posi.eprocess_address, "_EPROCESS");
    ASSERT_TRUE(eproc.get_address() == 0xfffffa80021bb430) << "wrong eproc addr";
    osi::i_t peb = eproc("Peb");
    ASSERT_TRUE(peb.get_address() == 0x7FFFFFDF000) << "wrong peb addr";
    osi::i_t LDR_DATA_TABLE = peb("Ldr");
    ASSERT_TRUE(LDR_DATA_TABLE.get_address() == 0x77BD3640) << "wrong ldr addr";
    osi::i_t ldr_list = LDR_DATA_TABLE["InLoadOrderModuleList"];
    ASSERT_TRUE(ldr_list.get_address() == 0x77BD3650) << "wrong ldr list addr";
    osi::i_t LDR_DATA_ENTRY = ldr_list("Flink").set_type("_LDR_DATA_TABLE_ENTRY");
    ASSERT_TRUE(LDR_DATA_ENTRY.get_address() == 0x002E2010)
        << "wrong ldr list entry addr";
    uint64_t fmodule_base = LDR_DATA_ENTRY["DllBase"].getu();
    uint64_t fmodule_size = LDR_DATA_ENTRY["SizeOfImage"].getu();

    fprintf(stderr, "fmodule_base: %lu\n", fmodule_base);
    fprintf(stderr, "fmodule_size: %lu\n", fmodule_size);

    s_kosi.system_vmem.reset();
    s_kosi.pmem->free(s_kosi.pmem);
}

int main(int argc, char** argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    if (argc != 2) {
        fprintf(stderr, "usage: %s amd64.raw\n", argv[0]);
        return 3;
    }

    testfile = argv[1];

    return RUN_ALL_TESTS();
}
