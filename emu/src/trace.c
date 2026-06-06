/* trace.c — map syscall trampoline addresses to names (the verified ABI). */
#include "trace.h"

const char *syscall_name(uint32_t a)
{
    switch (a) {
    case 0x1024: return "fs_op(idx9)";   /* OS 0x5B408, FAT/file cluster; benign at boot */
    case 0x1028: return "fclose";
    case 0x102C: return "fopen";
    case 0x1030: return "fread";
    case 0x1034: return "fseek";
    case 0x1038: return "fwrite";
    case 0x103C: return "closedir";
    case 0x1040: return "opendir";
    case 0x1044: return "readdir";
    case 0x1050: return "fflush/reset?";
    case 0x1054: return "get_ticks_ms";
    case 0x1058: return "input_read";
    case 0x105C: return "obj24_read";  /* OS 0x5E950: memcpy(buf,&sys,24) */
    case 0x1060: return "obj24_write"; /* OS 0x5E94C: 24-byte set (board model passed) */
    case 0x1064: return "rng";
    case 0x1070: return "get_boot_args";
    case 0x1074: return "rfid_inventory";
    case 0x1078: return "rfid_read_block";
    case 0x107C: return "rfid_write_block";
    case 0x1080: return "rfid_release?";
    case 0x1088: return "rfid_read_cached?";
    case 0x1094: return "audio_stop";
    case 0x1098: return "audio_start/buf";
    case 0x109C: return "audio_init";
    case 0x10A0: return "led/mode-vol";
    case 0x10E4: return "scheduler_init";
    case 0x10EC: return "yield";
    case 0x10F4: return "yield_block";
    case 0x10F8: return "queue_create";
    case 0x1108: return "queue_recv_to";
    case 0x110C: return "queue_recv_blk";
    case 0x1110: return "queue_send_to";
    case 0x1114: return "queue_send_try";
    case 0x1118: return "sem_take";
    case 0x111C: return "sem_create";
    case 0x112C: return "sem_take_try";
    case 0x1130: return "sem_give";
    case 0x113C: return "thread_create";
    case 0x1144: return "thread_exit";
    case 0x115C: return "sleep_ms";
    default:     return "?";
    }
}
