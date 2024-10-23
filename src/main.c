/* $Id: main.c,v 1.30 2008/12/11 12:18:17 ecd Exp $
 */

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdbool.h>
#include <string.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/time.h>
#include <time.h>
#include <errno.h>

#include <getopt.h>

#include <gtk/gtk.h>
#include <glib.h>

#include <memory.h>

#include "x49gp.h"
#include "x49gp_ui.h"
#include "s3c2410.h"
#include "x49gp_timer.h"

#include "gdbstub.h"

#ifndef VERSION_MAJOR
#  define VERSION_MAJOR 0
#endif
#ifndef VERSION_MINOR
#  define VERSION_MINOR 0
#endif
#ifndef PATCHLEVEL
#  define PATCHLEVEL 0
#endif

static x49gp_t* x49gp;

/* LD TEMPO HACK */
CPUState* __GLOBAL_env;

int semihosting_enabled = 1;

uint8_t* phys_ram_base;
int phys_ram_size;
ram_addr_t ram_size = 0x80000; // LD ???

/* vl.c */
int singlestep;

#if !( defined( __APPLE__ ) || defined( _POSIX_C_SOURCE ) && !defined( __sun__ ) )
static void* oom_check( void* ptr )
{
    if ( ptr == NULL )
        abort();

    return ptr;
}
#endif

void* qemu_memalign( size_t alignment, size_t size )
{
#if defined( __APPLE__ ) || defined( _POSIX_C_SOURCE ) && !defined( __sun__ )
    int ret;
    void* ptr;
    ret = posix_memalign( &ptr, alignment, size );
    if ( ret != 0 )
        abort();
    return ptr;
#elif defined( CONFIG_BSD )
    return oom_check( valloc( size ) );
#else
    return oom_check( memalign( alignment, size ) );
#endif
}

void qemu_init_vcpu( void* _env )
{
    CPUState* env = _env;

    env->nr_cores = 1;
    env->nr_threads = 1;
}

int qemu_cpu_self( void* env ) { return 1; }

void qemu_cpu_kick( void* env ) {}

void armv7m_nvic_set_pending( void* opaque, int irq ) { abort(); }
int armv7m_nvic_acknowledge_irq( void* opaque ) { abort(); }
void armv7m_nvic_complete_irq( void* opaque, int irq ) { abort(); }

void* qemu_malloc( size_t size ) { return malloc( size ); }

void* qemu_mallocz( size_t size )
{
    void* ptr;

    ptr = qemu_malloc( size );
    if ( NULL == ptr )
        return NULL;
    memset( ptr, 0, size );
    return ptr;
}

void qemu_free( void* ptr ) { free( ptr ); }

void* qemu_vmalloc( size_t size )
{
#if defined( __linux__ )
    void* mem;
    if ( 0 == posix_memalign( &mem, sysconf( _SC_PAGE_SIZE ), size ) )
        return mem;
    return NULL;
#else
    return valloc( size );
#endif
}

#define SWI_Breakpoint 0x180000

uint32_t do_arm_semihosting( CPUState* env )
{
    uint32_t number;
    if ( env->thumb ) {
        number = lduw_code( env->regs[ 15 ] - 2 ) & 0xff;
    } else {
        number = ldl_code( env->regs[ 15 ] - 4 ) & 0xffffff;
    }
    switch ( number ) {
        case SWI_Breakpoint:
            break;

        case 0:
#ifdef DEBUG_X49GP_SYSCALL
            printf( "%s: SWI LR %08x: syscall %u: args %08x %08x %08x %08x %08x %08x %08x\n", __FUNCTION__, env->regs[ 14 ], env->regs[ 0 ],
                    env->regs[ 1 ], env->regs[ 2 ], env->regs[ 3 ], env->regs[ 4 ], env->regs[ 5 ], env->regs[ 6 ], env->regs[ 7 ] );
#endif

#if 1
            switch ( env->regs[ 0 ] ) {
                case 305: /* Beep */
                    printf( "%s: BEEP: frequency %u, time %u, override %u\n", __FUNCTION__, env->regs[ 1 ], env->regs[ 2 ],
                            env->regs[ 3 ] );

                    gdk_beep();
                    env->regs[ 0 ] = 0;
                    return 1;

                case 28: /* CheckBeepEnd */
                    env->regs[ 0 ] = 0;
                    return 1;

                case 29: /* StopBeep */
                    env->regs[ 0 ] = 0;
                    return 1;

                default:
                    break;
            }
#endif
            break;

        default:
            break;
    }

    return 0;
}

void x49gp_set_idle( x49gp_t* x49gp, x49gp_arm_idle_t idle )
{
#ifdef DEBUG_X49GP_ARM_IDLE
    if ( idle != x49gp->arm_idle ) {
        printf( "%s: arm_idle %u, idle %u\n", __FUNCTION__, x49gp->arm_idle, idle );
    }
#endif

    x49gp->arm_idle = idle;

    if ( x49gp->arm_idle == X49GP_ARM_RUN ) {
        x49gp->env->halted = 0;
    } else {
        x49gp->env->halted = 1;
        cpu_exit( x49gp->env );
    }
}

static void arm_sighnd( int sig )
{
    switch ( sig ) {
        case SIGUSR1:
            //		stop_simulator = 1;
            //		x49gp->arm->CallDebug ^= 1;
            break;
        default:
            fprintf( stderr, "%s: sig %u\n", __FUNCTION__, sig );
            break;
    }
}

void x49gp_gtk_timer( void* data )
{
    while ( gtk_events_pending() ) {
        // printf("%s: gtk_main_iteration_do()\n", __FUNCTION__);
        gtk_main_iteration_do( false );
    }

    x49gp_mod_timer( x49gp->gtk_timer, x49gp_get_clock() + X49GP_GTK_REFRESH_INTERVAL );
}

void x49gp_lcd_timer( void* data )
{
    x49gp_t* x49gp = data;
    int64_t now, expires;

    // printf("%s: lcd_update\n", __FUNCTION__);
    x49gp_lcd_update( x49gp );
    gdk_flush();

    now = x49gp_get_clock();
    expires = now + X49GP_LCD_REFRESH_INTERVAL;

    // printf("%s: now: %lld, next update: %lld\n", __FUNCTION__, now, expires);
    x49gp_mod_timer( x49gp->lcd_timer, expires );
}

/***********/
/* OPTIONS */
/***********/

struct options {
    char* config;
    int debug_port;
    int start_debugger;
    char* firmware;
    x49gp_reinit_t reinit;

    int more_options;
};

struct options opt;

static void config_init( char* progname, int argc, char* argv[] )
{
    int option_index;
    int c = '?';

    opt.config = NULL;
    opt.debug_port = 0;
    opt.start_debugger = false;
    opt.reinit = X49GP_REINIT_NONE;
    opt.firmware = NULL;

    const char* optstring = "hrc:D:df:F";
    struct option long_options[] = {
        {"help",         no_argument,       NULL, 'h'},

        {"config", required_argument, NULL, 'c'},

        {"enable-debug", required_argument, NULL, 'D'},
        {"debug",        no_argument, NULL, 'd'},
        {"reflash",      required_argument, NULL, 'f'},
        {"reflash-full", no_argument, NULL, 'F'},
        {"reboot",       no_argument,       NULL, 'r'},

        {0,              0,                 0,    0  }
    };

    while ( c != EOF ) {
        c = getopt_long( argc, argv, optstring, long_options, &option_index );

        switch ( c ) {
            case 'h':
                fprintf( stderr,
                         "%s %i.%i.%i Emulator for HP 49G+ / 50G calculators\n"
                         "Usage: %s [<options>] [<config-file>]\n"
                         "Valid options:\n"
                         " -c, --config[=<filename>]     alternate config file\n"
                         " -D, --enable-debug[=<port>]   enable the debugger interface\n"
                         "                               (default port: %u)\n"
                         " -d, --debug                   use along -D to also start the"
                         " debugger immediately\n"
                         " -f, --reflash[=firmware]      rebuild the flash using the"
                         " supplied firmware\n"
                         "                               (default: select one"
                         " interactively)\n"
                         "                               (implies -r for safety"
                         " reasons)\n"
                         " -F, --reflash-full            use along -f to drop the"
                         " flash contents\n"
                         "                               in the area beyond the"
                         " firmware\n"
                         " -r, --reboot                  reboot on startup instead of"
                         " continuing from the\n"
                         "                               saved state in the config"
                         " file\n"
                         " -h, --help                    print this message and exit\n"
                         "The config file is formatted as INI file and contains the"
                         " settings for which\n"
                         "persistence makes sense, like calculator model, CPU"
                         " registers, etc.\n"
                         "If the config file is omitted, ~/.config/%s/config is used.\n"
                         "Please consult the manual for more details on config file"
                         " settings.\n",
                         progname, VERSION_MAJOR, VERSION_MINOR, PATCHLEVEL, progname, DEFAULT_GDBSTUB_PORT, progname );
                exit( EXIT_SUCCESS );
                break;
        case 'r':
            if ( opt.reinit < X49GP_REINIT_REBOOT_ONLY )
                opt.reinit = X49GP_REINIT_REBOOT_ONLY;
            break;
        case 'c':
            opt.config = strdup( optarg );
            break;
        case 'D':
            {
                char* end;
                int port;

                if ( optarg == NULL && opt.debug_port == 0 )
                        opt.debug_port = DEFAULT_GDBSTUB_PORT;

                port = strtoul( optarg, &end, 0 );
                if ( ( end == optarg ) || ( *end != '\0' ) ) {
                    fprintf( stderr, "Invalid port \"%s\", using default\n", optarg );
                    if ( opt.debug_port == 0 )
                        opt.debug_port = DEFAULT_GDBSTUB_PORT;
                }

                if ( opt.debug_port != 0 && opt.debug_port != DEFAULT_GDBSTUB_PORT )
                    fprintf( stderr,
                            "Additional debug port \"%s\" specified,"
                            " overriding\n",
                            optarg );
                opt.debug_port = port;
            }
            break;
        case 'd':
            opt.start_debugger = true;
            break;
        case 'F':
            opt.reinit = X49GP_REINIT_FLASH_FULL;
            break;
        case 'f':
            if ( opt.reinit < X49GP_REINIT_FLASH )
                opt.reinit = X49GP_REINIT_FLASH;

            if ( opt.firmware != NULL )
                fprintf( stderr,
                        "Additional firmware file \"%s\" specified,"
                        " overriding\n",
                        optarg );
            opt.firmware = optarg;
            break;
        default:
            break;
        }
    }
}
/************/
/* \OPTIONS */
/************/

void ui_sighnd( int sig )
{
    switch ( sig ) {
        case SIGINT:
        case SIGQUIT:
        case SIGTERM:
            x49gp->arm_exit = 1;
            cpu_exit( x49gp->env );
            break;
    }
}

int main( int argc, char** argv )
{
    char *progname, *progpath;
    int error;
    const char* home;

    progname = g_path_get_basename( argv[ 0 ] );
    progpath = g_path_get_dirname( argv[ 0 ] );

    gtk_init( &argc, &argv );

    config_init( progname, argc, argv );

    x49gp = malloc( sizeof( x49gp_t ) );
    if ( NULL == x49gp ) {
        fprintf( stderr, "%s: %s:%u: Out of memory\n", progname, __FUNCTION__, __LINE__ );
        exit( 1 );
    }
    memset( x49gp, 0, sizeof( x49gp_t ) );

#ifdef DEBUG_X49GP_MAIN
    fprintf( stderr, "_SC_PAGE_SIZE: %08lx\n", sysconf( _SC_PAGE_SIZE ) );

    printf( "%s:%u: x49gp: %p\n", __FUNCTION__, __LINE__, x49gp );
#endif

    INIT_LIST_HEAD( &x49gp->modules );

    x49gp->progname = progname;
    x49gp->progpath = progpath;
    x49gp->clk_tck = sysconf( _SC_CLK_TCK );

    x49gp->emulator_fclk = 75000000;
    x49gp->PCLK_ratio = 4;
    x49gp->PCLK = 75000000 / 4;

    // cpu_set_log(0xffffffff);
    cpu_exec_init_all( 0 );
    x49gp->env = cpu_init( "arm926" );
    __GLOBAL_env = x49gp->env;

    //	cpu_set_log(cpu_str_to_log_mask("all"));

    x49gp_timer_init( x49gp );

    x49gp->gtk_timer = x49gp_new_timer( X49GP_TIMER_REALTIME, x49gp_gtk_timer, x49gp );
    x49gp->lcd_timer = x49gp_new_timer( X49GP_TIMER_VIRTUAL, x49gp_lcd_timer, x49gp );

    x49gp_ui_init( x49gp );

    x49gp_s3c2410_arm_init( x49gp );

    x49gp_flash_init( x49gp );
    x49gp_sram_init( x49gp );

    x49gp_s3c2410_init( x49gp );

    if ( x49gp_modules_init( x49gp ) ) {
        exit( 1 );
    }

    if ( opt.config == NULL ) {
        char config_dir[ strlen( progname ) + 9 ];

        home = g_get_home_dir();
        sprintf( config_dir, ".config/%s", progname );
        opt.config = g_build_filename( home, config_dir, "config", NULL );
    }

    x49gp->basename = g_path_get_dirname( opt.config );
    x49gp->debug_port = opt.debug_port;
    x49gp->startup_reinit = opt.reinit;
    x49gp->firmware = opt.firmware;

    error = x49gp_modules_load( x49gp, opt.config );
    if ( error || opt.reinit >= X49GP_REINIT_REBOOT_ONLY ) {
        if ( error && error != -EAGAIN ) {
            exit( 1 );
        }
        x49gp_modules_reset( x49gp, X49GP_RESET_POWER_ON );
    }
    // x49gp_modules_reset(x49gp, X49GP_RESET_POWER_ON);

    signal( SIGINT, ui_sighnd );
    signal( SIGTERM, ui_sighnd );
    signal( SIGQUIT, ui_sighnd );

    signal( SIGUSR1, arm_sighnd );

    x49gp_set_idle( x49gp, 0 );

    // stl_phys(0x08000a1c, 0x55555555);

    x49gp_mod_timer( x49gp->gtk_timer, x49gp_get_clock() );
    x49gp_mod_timer( x49gp->lcd_timer, x49gp_get_clock() );

    if ( opt.debug_port != 0 && opt.start_debugger ) {
        gdbserver_start( opt.debug_port );
        gdb_handlesig( x49gp->env, 0 );
    }

    x49gp_main_loop( x49gp );

    x49gp_modules_save( x49gp, opt.config );
    x49gp_modules_exit( x49gp );

#if 0
        printf("ClkTicks: %lu\n", ARMul_Time(x49gp->arm));
        printf("D TLB: hit0 %lu, hit1 %lu, search %lu (%lu), walk %lu\n",
                x49gp->mmu->dTLB.hit0, x49gp->mmu->dTLB.hit1,
                x49gp->mmu->dTLB.search, x49gp->mmu->dTLB.nsearch,
                x49gp->mmu->dTLB.walk);
        printf("I TLB: hit0 %lu, hit1 %lu, search %lu (%lu), walk %lu\n",
                x49gp->mmu->iTLB.hit0, x49gp->mmu->iTLB.hit1,
                x49gp->mmu->iTLB.search, x49gp->mmu->iTLB.nsearch,
                x49gp->mmu->iTLB.walk);
#endif
    return 0;
}
