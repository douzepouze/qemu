 /*
 * QTest testcase for Microbit board using the Nordic Semiconductor nRF51 SoC
 *
 * Copyright (c) 2018 Steffen Görtz <contrib@steffen-goertz.de>
 */


#include "qemu/osdep.h"
#include "libqtest.h"
//#include "qemu-common.h"
//#include "qemu/sockets.h"
//#include "qemu/iov.h"
//#include "qemu/bitops.h"


#define FLASH_SIZE          (256 * 1024)
#define NVMC_BASE           0x4001E000UL
#define NVMC_READY          0x400
#define NVMC_CONFIG         0x504
#define NVMC_ERASEPAGE      0x508
#define NVMC_ERASEPCR1      0x508
#define NVMC_ERASEALL       0x50C
#define NVMC_ERASEPCR0      0x510
#define NVMC_ERASEUICR      0x514


static char tmp_path[] = "/tmp/qtest.microbit.XXXXXX";


static void test_nrf51_nvmc(void) {
    uint32_t value;
//    /* Test always ready */
//    value = readl(NVMC_BASE + NVMC_READY);
//    g_assert_cmpuint(value & 0x01, ==, 0x01);

    /* Test write-read config register */
    writel(NVMC_BASE+NVMC_CONFIG, 0x03);
    value = readl(NVMC_BASE + NVMC_READY);
    g_assert_cmpuint(value, ==, 0x03);
//
//    for (i = 0; i < BIN_SIZE; ++i) {
//        memory_content[i] = qtest_readb(s, base_addr + i);
//        g_assert_cmpuint(memory_content[i], ==, pre_store[i]);
//    }
//    qtest_memread(global_qtest, addr, data, size);
//    g_assert_cmphex(buf[i], ==, (uint8_t)i);
}



int main(int argc, char **argv)
{
    int ret;
    int fd;

    g_test_init(&argc, &argv, NULL);

    fd = mkstemp(tmp_path);
    g_assert(fd >= 0);
    ret = ftruncate(fd, FLASH_SIZE);
    g_assert(ret == 0);
    close(fd);

    global_qtest = qtest_startf("-machine microbit -kernel /home/pouze/projects/gsoc/test_firmware/microbit-micropython/microbit-micropython-e10a5ffdbaf1.elf");

    qtest_add_func("/microbit/nrf51/nvmc", test_nrf51_nvmc);

    ret = g_test_run();

    qtest_quit(global_qtest);
//    unlink(tmp_path);
    return ret;
}
