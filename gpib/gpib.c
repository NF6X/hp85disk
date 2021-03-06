/**
 @file gpib/gpib.c

 @brief GPIB emulator for HP85 disk emulator project for AVR.

 @par Edit History - [1.0]   [Mike Gore]  Initial revision of file.

 @par Copyright &copy; 2014-2020 Mike Gore, All rights reserved. GPL
 @see http://github.com/magore/hp85disk
 @see http://github.com/magore/hp85disk/COPYRIGHT.md for Copyright details

@par Based on work by Anders Gustafsson.

@par Copyright &copy; 2014 Anders Gustafsson All rights reserved..

*/

#include "user_config.h"
#include "hal.h"
#include "gpib_hal.h"

#include "defines.h"
#include "gpib.h"
#include "gpib_task.h"
#include "amigo.h"
#include "ss80.h"

#include "fatfs.h"
#include "posix.h"
#include "debug.h"

/// - References: Documenation and related sources of information
///  - Web Resources:
///    - http://www.hp9845.net
///    - http://www.hpmuseum.net
///    - http://www.hpmusuem.org
///    - http://bitsavers.trailing-edge.com
///    - http://en.wikipedia.org/wiki/IEEE-488
///
/// - SS80 References:
///   - "SS80" is the short form used in the project
///   - "Subset 80 from Fixed and flexible disc drives"
///   - Printed November, 1985
///   - HP Part# 5958-4129
///
/// - CS80 References:
///   - "CS80" is the short form used in the project
///   - "CS/80 Instruction Set Programming Manual"
///   - Printed: APR 1983
///   - HP Part# 5955-3442
///
/// - Amigo References:
///   - "Amigo" is the short form used in the project
///   - "Appendix A of 9895A Flexible Disc Memory Service Manual"
///   - HP Part# 09895-90030
///
/// - HP-IB
///   - "HP-IB" is the short form used in the project
///   - "Condensed Description of the Hewlett Packard Interface Bus"
///   - Printed March 1975
///   - HP Part# 59401-90030
///
/// - Tutorial Description of The Hewllet Packard Interface Bus"
///   - "HP-IB Tutorial" is the short form used in the project
///   - http://www.hpmemory.org/an/pdf/hp-ib_tutorial_1980.pdf
///     - Printed January 1983
///   - http://www.ko4bb.com/Manuals/HP_Agilent/HPIB_tutorial_HP.pdf
///     - Printed 1987
///
/// - GPIB / IEEE 488 Tutorial by Ian Poole
///   - http://www.radio-electronics.com/info/t_and_m/gpib/ieee488-basics-tutorial.php

///@brief user task called in GPIB read/write byte functions
extern void gpib_user_task();

/// @brief common IO buffer for  gpib_read_str() and gpib_write_str()
uint8_t gpib_iobuff[GPIB_IOBUFF_LEN];

/// @brief gpib_unread() flag
uint8_t gpib_unread_f = 0;                        // saved character flag
/// @brief gpib_unread() data
uint16_t gpib_unread_data;                        // saved character and status

/// @brief gpib talk address
uint8_t talking;
/// @brief gpib last talk address
uint8_t talking_last;

/// @brief gpib listen address
uint8_t listening;
/// @brief gpib last listen address
uint8_t listening_last;

/// @brief gpib serial poll status
uint8_t spoll;
/// @brief gpib current and last command
uint16_t lastcmd,current;

/// @brief gpib secondary
uint8_t secondary;

/// @brief GPIB command mapping to printable strings
typedef struct
{
    int cmd;
    char *name;
} gpib_token_t;

gpib_token_t gpib_tokens[] =
{
    {0x01,"GTL" },
    {0x04,"SDC" },
    {0x05,"PPC" },
    {0x08,"GET" },
    {0x09,"TCT" },
    {0x11,"LLO" },
    {0x14,"DCL" },
    {0x15,"PPU" },
    {0x18,"SPE" },
    {0x19,"SPD" },
    {0x3F,"UNL" },
    {0x5F,"UNT" },
    {-1,NULL }
};

///  See gpib_hal.c for CPU specific code
///  Provide Elapsed timer and Timeout timer - both in Microseconds

/// @brief  Start measuring time - used with hpib_timer_elapsed_end()
///
///  We use the lower level clock elapsed timer functions here
/// @return  void
void gpib_timer_elapsed_begin( void )
{

#ifdef SYSTEM_ELAPSED_TIMER
    clock_elapsed_begin();
#else
    cli();
    gpib_timer.elapsed = 0;
    sei();
#endif
}


/// @brief  Reset elapsed and timeout timers
///    Elapses and Timeout Timers
/// @return  void

void gpib_timer_reset( void )
{
    cli();
    gpib_timer.elapsed = 0;
    gpib_timer.down_counter = 0;
    gpib_timer.down_counter_done = 1;
    sei();
}


/// @brief Display user message and time delta since gpib_timer_elapsed_begin() call
///
///   Display a message
/// @return  void
void gpib_timer_elapsed_end( char *msg)
{
#ifdef SYSTEM_ELAPSED_TIMER
    clock_elapsed_end( msg );
#else
    uint32_t val;
    uint32_t seconds;
    uint32_t useconds;

    cli();
    val  = gpib_timer.elapsed;
    sei();

    seconds = val * GPIB_TASK_TIC_US / 1000000L;
    useconds = (val * GPIB_TASK_TIC_US ) % 1000000L;

    printf("[%s: %ld.%06ld]\n", msg, seconds,useconds);
#endif
}


/// @brief  Main GPIB timer task called by low level interrup hander
///
/// - Provides Down timers and Elapsed timers
/// - Called every GPIB_TASK_TIC_US Micro Seconds
/// @return  void
void gpib_timer_task()
{
    cli();
#ifndef SYSTEM_ELAPSED_TIMER
    gpib_timer.elapsed++;
#endif
    if(gpib_timer.down_counter)
        gpib_timer.down_counter--;
    else
        gpib_timer.down_counter_done = 1;
    sei();
}


/// @brief  Set GPIB timeout timer in units of GPIB_TASK_TIC_US
/// @see: gpib_timeout_test()
/// @return  void
void gpib_timeout_set(uint32_t time)
{
// printf("\n%8ld\n", (long)time);
    cli();
    gpib_timer.down_counter = time;
    gpib_timer.down_counter_done = 0;
    sei();
}


/// @brief  Test GPIB timeout timer for timeout condition
/// @see: gpib_timeout_set()
/// @return  1 if timeout, 0 if not
uint8_t gpib_timeout_test()
{
// printf("%8ld,%d\r", (long)gpib_timer.down_counter, (int)gpib_timer.down_counter_done);
    return(gpib_timer.down_counter_done);
}

/// @brief  Initialize/Release  GPIB Bus control lines 
/// Used for Power ON, Reset or IFC LOW reset conditions
/// @return  void
void gpib_bus_init( )
{

	uint8_t sreg = SREG;
	cli();
///FIXME verify resetting gpib_unread_f is ALWAYS correct when called
    gpib_unread_f = 0;

    GPIB_BUS_IN();                                // CPU data direction IN
    GPIB_BUS_LATCH_WR(0xff);                      // Float them HIGH

	// ============
	// Handshake lines
	// TX, TE = 1, DAV = T, NDAC = R, NRFD = R
	// RX, TE = 0, DAV = R, NDAC = T, NRFD = T
    GPIB_PIN_FLOAT_UP(DAV);                       // DAV FLOAT PULLUP
    GPIB_PIN_FLOAT_UP(NRFD);                      // SRQ FLOAT PULLUP
    GPIB_PIN_FLOAT_UP(NDAC);                      // NDAC FLOAT PULLUP
	// ============

	// ============
	// DC = 1
	// Always Inputs
	// We will NEVER write to ATN,IFC,REN,SRQ after BUS init
    GPIB_PIN_FLOAT_UP(ATN);                       // ATN FLOAT PULLUP
    GPIB_PIN_FLOAT_UP(IFC);                       // IFC FLOAT PULLUP
    GPIB_PIN_FLOAT_UP(REN);                       // REN FLOAT PULLUP
	// Always OC output
    GPIB_PIN_FLOAT_UP(SRQ);                       // SRQ FLOAT PULLUP
	// ============

	// ============
	// EOI
	// TX if TE = 1, ATN = 1
	// RX if TE = 1, ATN = 0
	// RX if TE = 0, ATN = 1
    GPIB_PIN_FLOAT_UP(EOI);                       // EOI FLOAT PULLUP
	// ============

#if BOARD == 2
	// ALWAYS in device mode
	// SC = 0, DC = 1 
    GPIB_IO_LOW(SC);                              // REN IN, IFC IN
    GPIB_IO_HI(DC);                               // ATN IN, EOI IN, SRQ OUT OC
	// Always OC mode in case parallel poll happens
	// PE= 0, OC BUS transciever mode
    GPIB_IO_LOW(PE);                              // Enable OC GPIB BUS transceivers
	// TE is the ONLY SN75160B/SN75162B control bit we use in device mode
	// RX = 0, TX = 1
    GPIB_IO_LOW(TE);                              // BUS IN, DAV IN, NDAC OUT OC, NRFD OUT OC
#endif

    GPIB_BUS_SETTLE();                            // Let Data BUS settle

	SREG = sreg;

#if SDEBUG
    if(debuglevel & GPIB_BUS_OR_CMD_BYTE_MESSAGES)
        printf("[GPIB BUS_INIT]\n");
#endif

}



/// @brief  Initialize GPIB Bus control lines for READ
/// - Set busy = 0 after powerup everying floating or high
///  - If busy = 1 NRFD/NDAC are set to busy low
/// - References:
///  - HP-IB Tutorial
///  -  HP-IB pg 8-11
/// @return  void
// This function sets the following states
// GPIB_BUS_LATCH_WR(0xff); // float BUS 
// GPIB_IO_LOW(TE);			// BUS IN
// BUS IN, DAV IN, NDAC OUT, NRFD OUT
// ATN IN, EOI IN, SRQ OUT OC
// ============
void gpib_rx_init(uint8_t busy)
{
	uint8_t sreg = SREG;
	cli();
    GPIB_BUS_IN();                                // CPU data direction IN
    GPIB_BUS_LATCH_WR(0xff);                      // Float them HIGH

	// RX, TE = 0, DAV = R, NDAC = T, NRFD = T
    GPIB_PIN_FLOAT_UP(DAV);                       // DAV FLOAT PULLUP

    GPIB_PIN_FLOAT_UP(EOI);						  // EOI FLOAT PULLUIP

//FIXME
// If ATN = 0, then set NRFD = 0, NDAC = 0
// (SPecification says within 200 nanoseconds) 
// To permit three-wire handshake 

	if(GPIB_PIN_TST(ATN) == 0 || busy)
	{
#if BOARD == 2
		GPIB_IO_LOW(TE);                          // BUS IN, DAV IN, NDAC OUT, NRFD OUT
#endif
		GPIB_IO_LOW(NDAC);
        GPIB_IO_LOW(NRFD);

	}
	if(!busy)
	{
		GPIB_PIN_FLOAT_UP(NRFD);                  // OC PULLUP
		GPIB_PIN_FLOAT_UP(NDAC);                  // OC PULLUP
#if BOARD == 2
		GPIB_IO_LOW(TE);                          // BUS IN, DAV IN, NDAC OUT, NRFD OUT
#endif

	}
	SREG = sreg;
}



/// @brief  Initialize GPIB Bus control lines for WRITE
/// - Set busy = 0 after powerup everying floating or high
///  - If busy = 1 NRFD/NDAC are set to busy low
/// - References:
///  - HP-IB Tutorial
///  -  HP-IB pg 8-11
/// @return  void
// This function sets the following states
// GPIB_BUS_LATCH_WR(0xff); // float BUS 
// GPIB_IO_HI(TE);			// BUS OUT with OC PULLUP
// BUS IN, DAV OUT, NDAC IN, NRFD IN
// ATN IN, EOI IN/OUT, SRQ OUT OC
// ============
// EOI IN/OUT
// TX if TE = 1, ATN = 1
// RX if TE = 1, ATN = 0
// NOTE: IF ATN goes low when writing when should abort!
void gpib_tx_init()
{
	uint8_t sreg = SREG;
	cli();
    GPIB_BUS_IN();                                // CPU data direction IN
    GPIB_BUS_LATCH_WR(0xff);                      // Float them HIGH
	// TX, TE = 1, DAV = T, NDAC = R, NRFD = R
    GPIB_PIN_FLOAT_UP(DAV);                       // DAV FLOAT PULLUP
    GPIB_PIN_FLOAT_UP(NRFD);                      // SRQ FLOAT PULLUP
    GPIB_PIN_FLOAT_UP(NDAC);                      // NDAC FLOAT PULLUP

    GPIB_PIN_FLOAT_UP(EOI);						  // EOI FLOAT PULLUIP
#if BOARD == 2
	GPIB_IO_HI(TE);
#endif
    GPIB_BUS_OUT();
	SREG = sreg;
}


/// @brief  Reset GPIB states and related variables
///
/// - Called at powerup and IFC or reset states.
/// - Does not reset BUS
/// @return  void
void gpib_state_init( void )
{
#if SDEBUG
    if(debuglevel & GPIB_BUS_OR_CMD_BYTE_MESSAGES)
        printf("[GPIB STATE INIT]\n");
#endif
// Disable Parallel Poll Response
    ppr_init();

    listen_cleanup();

    talk_cleanup();

    spoll = 0;                                    // SPOLL disabled
    talking = 0;                                  // Listening/Talking State
    talking_last = 0;
    listening = 0;
    listening_last  = 0;
    lastcmd = 0;
    current = 0;
    secondary = 0;
}


///
/// - Reference: SS80 pg 3-4, section 3-3
/// @return  void
void gpib_enable_PPR(int bit)
{
    if(bit < 0 || bit > 7)
    {
        printf("gpib_enable_PPR: bit %d out of range\n", (int) bit);
        return;
    }
    ppr_bit_set(bit);
#if SDEBUG
    if(debuglevel & GPIB_PPR)
        printf("[EPPR bit:%d, mask:%02XH]\n",0xff & bit , 0xff & ppr_reg());
#endif
}


/// @brief Disable PPR (Parallel Poll Response) for a device
///
/// - Reference: SS80 pg 3-4, section 3-3
/// @return  void

void gpib_disable_PPR(int bit)
{
    if(bit < 0 || bit > 7)
    {
        printf("gpib_disable_PPR: bit %d out of range\n", (int) bit);
        return;
    }
    ppr_bit_clr(bit);
#if SDEBUG
    if(debuglevel & GPIB_PPR)
        printf("[DPPR bit:%d, mask:%02XH]\n",0xff & bit, 0xff & ppr_reg());
#endif
}


/// @brief  Attempt to detect the Parallel Poll Reposnse state
/// Used only for debugging - it is unlikely that we will catch this state
/// V2 hardware - this does not work unless reading BUS
///  - PPR is short for "Parellel Poll Response"
///  - PPR happens when the CIC (controller in charge) holds
///     BOTH of ATN == 0 and EOI == 0
///  - PPR Response happens using hardware
///       The hardware pulls a bit low on the GPIB bus corresponding to the device.
///       But only if any PPR mask bits are set in the hardware
///  - Note: EOI is never used in command mode (ATN = 0)
///        1) Hardware will automatically hold GPIB DATA BUS lines low
///  - ppr_reg() determines if we have PPR enabled.
///  - References:
///     - SS80 pg 3-4, section 3-3
/// @see gpib_enable_PPR()
/// @see gpib_disable_PPR()
/// @return  1 if we see a PPR, 0 if not

uint8_t gpib_detect_PP()
{
    uint8_t ddr, pins;

///FIXME we can only read EOI in bus READ mode
    if(GPIB_PIN_TST(ATN) == 0 && GPIB_PIN_TST(EOI) == 0 )
    {

#if SDEBUG
        if(debuglevel & GPIB_PPR)
            gpib_timer_elapsed_begin();
#endif

        if(debuglevel & GPIB_PP_BUS_STATUS)
        {
			///@brief Bus pin states but do not alter port direction state
			// Versions 2 hardware must have bus in READ mode
			// If not - then data reported wll not be correct 
			//  - harmless - debugging only
            pins = GPIB_PPR_RD(); // Reads Pin states without updating data direction

///@brief debugging - read the ddr data direction bits to determine read/write state
            ddr = GPIB_PPR_DDR_RD();
#if SDEBUG
            if(debuglevel & GPIB_PPR)
                printf("[PPR:%02XH, PIN:%02XH, DDR:%02XH]\n",
                    0xff & ppr_reg(), 0xff & pins, 0xff & ddr );
#endif
        }
		// Wait for the PPR state to end as there is no handshake
		///FIXME we can only read EOI in bus READ mode
		///  FIXME We add a test for read/write state ??
        while(GPIB_PIN_TST(ATN) == 0 && GPIB_PIN_TST(EOI) == 0 )
        {
            if(uart_keyhit(0))
			{
#if 0
				if(debuglevel & GPIB_ERR)
					printf("gpib_detect_PP: ATN=0 EOI=0\n");
#endif
				break;
			}

			// IFC is always in for a device
            if(GPIB_PIN_TST(IFC) == 0)
            {
				// IFC test and gpib_bus_init() is tested 
				// in every state machine loop
				// So not needed here
				// gpib_bus_init();
				if(debuglevel & GPIB_ERR)
					printf("gpib_detect_PP: IFC\n");
                break;
            }
        }
#if SDEBUG
        if(debuglevel & GPIB_PPR)
            gpib_timer_elapsed_end("PP released");
#endif
        return(1);
    }
    return(0);
}



/// @brief  GPIB ungets one character and all status states
///
///  Pushes a character back into the read buffer
/// @param[in] ch
///   - Lower 8 bits: Data or Command.
///     - If ATN is LOW then we strip parity from the byte.
///   - Upper 8 bits: Status and Errors present.
///     - @see gpib.h _FLAGS defines for a full list.
/// @return ch
uint16_t gpib_unread(uint16_t ch)
{
    if(!gpib_unread_f)
    {
        gpib_unread_data = ch;
        gpib_unread_f = 1;
    }
    else
    {
        if(debuglevel & (GPIB_ERR + GPIB_BUS_OR_CMD_BYTE_MESSAGES))
            printf("gpib_unread: error, can only be called once!\n");
    }
    return(ch);
}


/// @brief Read GPIB data BUS only
/// @return  bus (lower 8 bits)
uint8_t gpib_bus_read()
{
    uint8_t bus = ~(GPIB_BUS_RD());

///@brief if a command byte (ATN low) then strip partity
    if(!GPIB_PIN_TST(ATN))
        bus &= 0x7f;
    else
        bus &= 0xff;
    return(bus);
}


/// @brief  Read GPIB control lines only
/// FIXME V2 boards can only read pins enabled for read by the SN75162
///   FIXME We could add a test for read/write state ??
/// @return  control lines (upper 8 bits)
/// @see gpib_rx_init()
uint16_t gpib_control_pin_read()
{
    uint16_t control = 0;
    if(GPIB_PIN_TST(ATN) == 0 )
        control |= ATN_FLAG;
    if(GPIB_PIN_TST(EOI) == 0 )
        control |= EOI_FLAG;
    if(GPIB_PIN_TST(SRQ) == 0 )
        control |= SRQ_FLAG;
    if(GPIB_PIN_TST(REN) == 0 )
        control |= REN_FLAG;
    if(GPIB_PIN_TST(IFC) == 0 )
        control |= IFC_FLAG;
    return(control);
}


/// @brief  Read GPIB handshake lines only
/// FIXME: V2 boards can only read pins enabled for read by the SN75162
///   FIXME We could add a test for read/write state ??
/// @return  handshake lines (upper 8 bits)
uint16_t gpib_handshake_pin_read()
{
    uint16_t control = 0;
///@brief for tracing we can reuse the error flag bit values for DAV,NRFD and NDAC
/// FYI: This has no impact on the gpib_read_byte() functions and return values
    if(GPIB_PIN_TST(DAV) == 0 )
        control |= DAV_FLAG;
    if(GPIB_PIN_TST(NRFD) == 0 )
        control |= NRFD_FLAG;
    if(GPIB_PIN_TST(NDAC) == 0 )
        control |= NDAC_FLAG;
    return(control);
}


/// @brief  Send 1 byte and control line states to GPIB BUS
///
/// - **NOTE We are ONLY called via gpib_write_str**
/// - We have been addressed by the active controller to talk
///   We always arrive here in READ read state.
///   - Any error flags set on return imply the data was not likely sent
///   - You can OR the control flags ATN_FLAG, EOI_FLAG with (ch)
///     to send them these states.
///   - Results can be displayed for debugging with the decode(ch) funnction
///   - We always exit with NRFD and NDAC LOW
/// - References: HP-IB Tutorial pg13.
///   HP-IB Tutorial pg 13 for the receive and send control line states
///   Correction: The send routine MUST also wait for NDAC LOW before exit.
///   (You can verify this by checking the receive part of the handshake
///   diagram just before it returns to the start of the loop)
///   Failing to wait will cause problems that may masqurade as timing issues.
/// - (It caused problems with my HP85 before I added the code)
///
/// @param[out] ch: ( Data or Command ) and control flags
/// - Upper 8 bits: contril flags
/// - Flags:
///  - EOI_FLAG
///  - ATN_FLAG
///    - EOI and ATN can only be used in controller mode
///
/// @return
///   - Lower 8 bits: Data or Command.
///     - If ATN is LOW then we strip parity from the byte.
///   - Upper 8 bits: Status and Errors.
///     - @see gpib.h _FLAGS defines for a full list.
///     - An error implies the data byte can't be trusted
///     - Control Line Flags.
///       - EOI_FLAG
///       - SRQ_FLAG
///       - ATN_FLAG
///       - REN_FLAG
///       - PP_FLAG
///     - Error Flags:
///       - IFC_FLAG
uint16_t gpib_write_byte(uint16_t ch)
{
    uint8_t tx_state;

// We are always in READ state at this point 
// No need to initialize as READ mode
// Now Done in write_str
#if 0
	gpib_rx_init(0);	// NOT busy, NRFD and NDAC OC
#endif

    tx_state = GPIB_TX_START;
    gpib_timeout_set(HTIMEOUT);
    while(tx_state != GPIB_TX_DONE )
    {
	// Not called for writting
#if 0
        gpib_user_task();
#endif

		if(uart_keyhit(0))
		{
#if 0
			if(debuglevel & GPIB_ERR)
				printf("gpib_write_byte: KEY state=%d\n", tx_state);
#endif
			break;
		}

#if 0
// FIXME - this test used to break write:
// We assume that ATN and EOI are always IN during this test
// Try to detect PPR - only for debugging
        if(gpib_detect_PP())
            ch |= PP_FLAG;
#endif

// IFC is always in for a device
        if(GPIB_PIN_TST(IFC) == 0)
        {
            ch |= IFC_FLAG;
            gpib_bus_init();
			if(debuglevel & GPIB_ERR)
				printf("gpib_write_byte: IFC state=%d\n", tx_state);
            break;
        }

        switch(tx_state)
        {
			// DAV == 1 the bus is ready
            case GPIB_TX_START:
				gpib_tx_init();
				GPIB_PIN_FLOAT_UP(DAV);
                GPIB_BUS_SETTLE();                // Let Data BUS settle

                gpib_timeout_set(HTIMEOUT);
                tx_state = GPIB_TX_PUT_DATA;
                break;

// Wait for NRFD or NDAC LOW
            case GPIB_TX_WAIT_FOR_NRFD_OR_NDAC_LOW:
                if(GPIB_PIN_TST(NRFD) == 0 || GPIB_PIN_TST(NDAC) == 0)
                {
					if(GPIB_PIN_TST(ATN) == 1)
					{
						gpib_timeout_set(HTIMEOUT);
						tx_state = GPIB_TX_PUT_DATA;
					}
					else
					{
#ifdef SDEBUG
						if(debuglevel & GPIB_ERR)
							printf("gpib_write_byte: ATN = 0 while waiting for NRFD LOW state =%d\n",tx_state);
#endif
					}
					break;
                }
                if (gpib_timeout_test())
                {
                    if(debuglevel & (GPIB_ERR + GPIB_BUS_OR_CMD_BYTE_MESSAGES))
                        printf("<gpib_write_byte timeout waiting for NRFD==1 && NDAC == 0>\n");
                    ch |= TIMEOUT_FLAG;
                    tx_state = GPIB_TX_ERROR;
                    break;
                }
                break;

// Write Data
            case GPIB_TX_PUT_DATA:
                if(ch & EOI_FLAG)
                    GPIB_IO_LOW(EOI);
                else
                    GPIB_PIN_FLOAT_UP(EOI);
                GPIB_BUS_WR((ch & 0xff) ^ 0xff);  // Write Data inverted
                GPIB_BUS_SETTLE();                // Let Data BUS settle

                gpib_timeout_set(HTIMEOUT);
                tx_state = GPIB_TX_WAIT_FOR_NRFD_HI;
                break;

// Wait for BOTH NRFD HI and NDAC LOW
            case GPIB_TX_WAIT_FOR_NRFD_HI:
#if 0
                if(GPIB_PIN_TST(NRFD) == 1 && GPIB_PIN_TST(NDAC) == 0)
#else
                if(GPIB_PIN_TST(NRFD))
#endif
                {
                    tx_state = GPIB_TX_SET_DAV_LOW;
                }
                if (gpib_timeout_test())
                {
                    if(debuglevel & (GPIB_ERR + GPIB_BUS_OR_CMD_BYTE_MESSAGES))
                        printf("<gpib_write_byte timeout waiting for NRFD==1 && NDAC == 0>\n");
                    ch |= TIMEOUT_FLAG;
                    tx_state = GPIB_TX_ERROR;
                    break;
                }
                break;

            case GPIB_TX_SET_DAV_LOW:
                GPIB_IO_LOW(DAV);
                GPIB_BUS_SETTLE();                
                gpib_timeout_set(HTIMEOUT);
                tx_state = GPIB_TX_WAIT_FOR_NDAC_HI;
                break;

///@brief ALL devices are ready
            case GPIB_TX_WAIT_FOR_NDAC_HI:
                if(GPIB_PIN_TST(NDAC) == 1)       // Byte byte accepted
                {
                    tx_state = GPIB_TX_SET_DAV_HI;
                    break;
                }
                if (gpib_timeout_test())
                {
                    ch |= TIMEOUT_FLAG;
                    tx_state = GPIB_TX_ERROR;
                    if(debuglevel & (GPIB_ERR + GPIB_BUS_OR_CMD_BYTE_MESSAGES))
                        printf("<gpib_write_byte timeout waiting for NDAC==1>\n");
                }
                break;

///@release BUS
            case GPIB_TX_SET_DAV_HI:
                GPIB_PIN_FLOAT_UP(DAV);
                GPIB_BUS_SETTLE();
                tx_state = GPIB_TX_FINISH;
                gpib_timeout_set(HTIMEOUT);
                break;

            case GPIB_TX_FINISH:

                tx_state = GPIB_TX_DONE;
                break;

            case GPIB_TX_ERROR:
				// Free BUS, BUSY on error
                gpib_rx_init(1);
                if(debuglevel & (GPIB_ERR + GPIB_BUS_OR_CMD_BYTE_MESSAGES))
                    printf("<GPIB TX TIMEOUT>\n");
                tx_state = GPIB_TX_DONE;
                break;

// FIXME do we want to be busy at this point
            case GPIB_TX_DONE:
                break;
        }
    }
    return(ch);
}


/*
DAV - Data Valid Used to Indicate the condition of the information on the Data (010)
	lines. Driven TRUE (low) by the source when data Is settled and valid and NRFD
	FALSE (high) has been sensed.
NRFD - Not Ready For Data Used to Indicate the condition of readiness of device(s)
	to accept data. An acceptor sets NRFD TRUE {low} to Indicate It Is not ready
	to accept data. It· sets this line FALSE (high) when It Is ready to accept data.
	However, the NRFD line to the source wlll not go high until all participating
	acceptors are ready to accept data.
NDAC - Not Data Accepted Used to Indicate the condition of acceptance of data
	by device(s). The acceptor sets NDAC TRUE (low) to indicate it has not accepted
	data. When It accepts data from the 010 lines, It will set Its NDAC line FALSE
	(high). However, the NDAC line to the source wlll not go high until the
	last/slowest participating· acceptor accepts the data..
*/


/// @brief  read 1 byte and control line status from GPIB BUS
/// @param[in] trace: if non-zero do full bus handshake trace of read
///
/// - References: HP-IB Tutorial pg 13, 14.
/// - Notes: Their diagram is a bit misleading - they start in the BUS init phase.
///   The init happens much earlier in other code - we NEVER want to do that
///   here or bad things will happen - we could step on the sender NRFD test
///   and false trigger it. We instead start with NRFD FLOAT, and NDAC LOW.
///   See loop in send diagram it has NDAC == 0 and NRFD == 1 in the return
///   to send (beginning) part of the loop.
/// - The HP-IB reference has an error:
///     Read ready actually starts when NRFD = 1 and NDAC = 0.
///   If you look at the Write state diagram that can be confirmed.
///
/// @return
///   - Lower 8 bits: Data or Command.
///     - If ATN is LOW then we strip parity from the byte.
///   - Upper 8 bits: Status and Errors.
///     - @see gpib.h _FLAGS defines for a full list.
///     - An error implies the data byte can't be trusted
///     - Control Line Flags.
///       - EOI_FLAG
///       - SRQ_FLAG
///       - ATN_FLAG
///       - REN_FLAG
///       - PP_FLAG
///     - Error Flags:
///       - IFC_FLAG
///
/// @return (data bus, control and error status)
uint16_t gpib_read_byte(int trace)
{
    uint8_t rx_state;
    uint16_t ch;
    uint16_t bus, control, control_last;
    extern uint8_t gpib_unread_f;
    extern uint16_t gpib_unread_data;

    ch = 0;
    control_last = 0;

	// Return unread - last read - data and control lines
    if(gpib_unread_f)
    {
		// FYI any unread data has been traced
        gpib_unread_f = 0;
        return(gpib_unread_data);
    }

	// We start and end gpib_read_byte() with NRFD and NDAC LOW 
	// When ATN goes LOW all devices must pull NRFD and NDAC lines LOW
	// within 200 nanoseconds to permit three-wire handshake 
	// ATN requirements are met because we are always reading in command mode
	// ATN = 0 = COmmand Mode
    gpib_rx_init(1);

	// gpib_rx_init(1) sets the following states
	// NRFD = 0 Not Ready for Data, NDAC = 0 Data Not Accepted
	// GPIB_IO_LOW(TE);			// BUS IN
	// BUS IN, DAV IN, NDAC OUT , NRFD OUT 
	// ATN IN, EOI IN, SRQ OUT OC

	///@brief V2 boards can NOT read ALL bits on the control bus at once
    if(trace)
    {
        control_last = gpib_control_pin_read();
        control_last |= gpib_handshake_pin_read();
        gpib_trace_display(control_last, TRACE_BUS);
    }

	gpib_timeout_set(HTIMEOUT);
    rx_state = GPIB_RX_START;
    while(rx_state != GPIB_RX_DONE)
    {

        // User task that is called while waiting for commands
        gpib_user_task();

        if(uart_keyhit(0))
		{
#if 0
			if(debuglevel & GPIB_ERR)
				printf("gpib_read_byte: state=%d\n", rx_state);
#endif
            break;
		}

// Try to detect parallel poll (PP) for debugging 
// This test raily detects PP
// Can only work on V1 hardware
#if BOARD == 1
        if(gpib_detect_PP())
            ch |= PP_FLAG;
#endif

		// IFC is alwayon IN always in device mode
        if(GPIB_PIN_TST(IFC) == 0)
        {
            ch |= IFC_FLAG;
			if(debuglevel & GPIB_ERR)
				printf("gpib_read_byte: IFC state=%d\n", rx_state);
            gpib_bus_init();
            break;
        }

        switch(rx_state)
        {

			///@brief DAV must be high
            case GPIB_RX_START:
				//DEBUG
				if (GPIB_PIN_TST(DAV) == 1)
				{
					GPIB_BUS_SETTLE();                // Let Data BUS settle
					GPIB_PIN_FLOAT_UP(NRFD);
					GPIB_BUS_SETTLE();                // Let Data BUS settle
					rx_state = GPIB_RX_WAIT_FOR_DAV_LOW;
				}
                if (gpib_timeout_test())
                {
                    ch |= TIMEOUT_FLAG;
                    rx_state = GPIB_RX_ERROR;
                }
                break;

			// Wait for Data Avalable without timeout
            case GPIB_RX_WAIT_FOR_DAV_LOW:
                if ( GPIB_PIN_TST(DAV) == 0 )
				{
					GPIB_BUS_SETTLE();                
                    rx_state = GPIB_RX_DAV_IS_LOW;
				}
                break;

			// Data is Avaliable
            case GPIB_RX_DAV_IS_LOW:
				GPIB_IO_LOW(NRFD); // BUSY
				GPIB_BUS_SETTLE();                

				// Read DATA and Control lines
				// gpib_bus_read() strips parity if ATN is low command state

                bus = gpib_bus_read();
                ch |= bus;

				///@brief V2 boards can NOT read all control bits at once
				///@brief NRFD,NDAC and SRQ are cirrently outputs

                control_last = gpib_control_pin_read();
                ch |= control_last;

				// In theory the control_last should not have changed
				// from the initial values. ONly the Data BUS
                if(trace)
                {
                    control_last |= gpib_handshake_pin_read();
                    gpib_trace_display(bus | control_last, TRACE_READ);
                }

				// Release NDAC to say we read the byte
                GPIB_PIN_FLOAT_UP(NDAC);
                GPIB_BUS_SETTLE();                // NDAC bus settle time
                gpib_timeout_set(HTIMEOUT);
                rx_state = GPIB_RX_WAIT_FOR_DAV_HI;
                break;

			///@brief Wait for DAV HI
            case GPIB_RX_WAIT_FOR_DAV_HI:
                if (GPIB_PIN_TST(DAV) == 1)
                {
					GPIB_BUS_SETTLE();
                    rx_state = GPIB_RX_DAV_IS_HI;
                }
                if (gpib_timeout_test())
                {
                    ch |= TIMEOUT_FLAG;
                    rx_state = GPIB_RX_ERROR;
                }
                break;

			///@brief Ready for next byte
            case GPIB_RX_DAV_IS_HI:
				GPIB_IO_LOW(NDAC);
				GPIB_BUS_SETTLE();
				// Now BOTH NDAC and NRFD are LOW
                rx_state = GPIB_RX_DONE;
                break;

            case GPIB_RX_ERROR:
				GPIB_IO_LOW(NRFD);
				GPIB_IO_LOW(NDAC);
                rx_state = GPIB_RX_DONE;
                break;

            case GPIB_RX_DONE:
                break;
        }

        if(trace)
        {
/// V2 boards can not read all control and handshake bits at once
/// FIXME We could add a test for read/write state ??
/// NRFD,NDAC SRQ are outputs durring write phase, but not at very start
            control = gpib_control_pin_read();
            control |= gpib_handshake_pin_read();
            if(control_last != control)
            {
                gpib_trace_display(control, TRACE_BUS);
                control_last = control;
            }
        }
    }

///  Note: see: HP-IB Tutorial pg 13
///  - Remember that NDAC and NRFD are now both LOW!
///  - The spec says to KEEP BOTH LOW when NOT ready to read otherwise
///    we may miss a transfer and cause a controller timeout!
///  - GPIB TX state expects NRFD LOW on entry or it is an ERROR!

    lastcmd = current;

    if(ch & ERROR_MASK || (ch & ATN_FLAG) == 0)
        current = 0;
    else
        current = ch & CMD_MASK;

    return (ch);
}


/// @brief  Displays help for gpib_decode() function
///
/// You would call this once at the start of a trace for example.
/// @see: gpib.h _FLAGS defines for a full list is control lines we track
/// @see: gpib_trace()
/// @return void
///@param[in] fo: FILE pointer or "stdout"
void gpib_decode_header( FILE *fo)
{
    if(fo == NULL)
        fo = stdout;

    fprintf(fo,"==============================\n");
    fprintf(fo,"GPIB bus state\n");
    fprintf(fo,"HH . AESRPITB gpib\n");
    fprintf(fo,"HH = Hex value of Command or Data\n");
    fprintf(fo,"   . = ASCII of XX only for 0x20 .. 0x7e\n");
    fprintf(fo,"     A = ATN\n");
    fprintf(fo,"      E = EOI\n");
    fprintf(fo,"       S = SRQ\n");
    fprintf(fo,"        R = REN\n");
    fprintf(fo,"         I = IFC\n");
    fprintf(fo,"          P = Parallel Poll seen\n");
    fprintf(fo,"           T = TIMEOUT\n");
    fprintf(fo,"            B = BUS_ERROR\n");
    fprintf(fo,"              GPIB commands\n");
}


/// @brief decode/display all control flags and data on the GPIB BUS
/// @param[in] status: data bus value (lower 8 bits) control bus (upper 8 bits)
/// @param[in] trace_state: level of trace detail
/// TRACE_DISABLE = normal bus and control status report from read state only
/// TRACE_READ    = trace  bus and control reporting from read state only
/// TRACE_BUS     = trace  control reporting from all non-read states, data bus values are omiited
/// Note: trace states add DAV,NRFD,NDAC and ommit PPR status, BUS error and timeout
///       given that gpib_read_byte() report these anyway
///   - Lower 8 bits: Data or Command.
///     - If ATN is LOW then we strip parity from the byte.
///   - Upper 8 bits: Status and Errors.
///     - @see gpib.h _FLAGS defines for a full list.
///     - An error implies the data byte can't be trusted
///     - Control Line Flags.
///       - EOI_FLAG
///       - SRQ_FLAG
///       - ATN_FLAG
///       - REN_FLAG
///       - PP_FLAG
///     - Error Flags:
///       - IFC_FLAG
/// @param[in] str: string pointer to store the decoded result in.
///
/// @see: gpib_read_byte()
/// @see: gpib_write_byte(int ch)
/// @see: gpib_decode_header()
///
/// @return  void
/// Note the bits we can read depends on what hardware verions we have
/// V2 hardware can only read status bits based on GPIB control buffer direction
///   FIXME We could add a test for read/write state ??
void gpib_trace_display(uint16_t status,int trace_state)
{
    char str[128];
    char *tmp= str;
    uint8_t bus = status & 0xff;
    extern FILE *gpib_log_fp;

    str[0] = 0;

// Display data bus ???
    if(trace_state == TRACE_DISABLE || trace_state == TRACE_READ)
    {
        uint8_t printable = ' ';                  // Data
        if( !(status & ATN_FLAG) && (bus >= 0x20 && bus <= 0x7e) )
            printable = bus;
        sprintf(str, "%02X %c ", (int)bus & 0xff, (int)printable);
    }
    else
    {
        sprintf(str, "     ");
    }

    tmp = str + strlen(str);

    if(status & ATN_FLAG)
        *tmp++ = 'A';
    else
        *tmp++ = '-';

    if(status & EOI_FLAG)
        *tmp++ = 'E';
    else
        *tmp++ = '-';

    if(status & SRQ_FLAG)
        *tmp++ = 'S';
    else
        *tmp++ = '-';

    if(status & REN_FLAG)
        *tmp++ = 'R';
    else
        *tmp++ = '-';

    if(status & IFC_FLAG)
        *tmp++ = 'I';
    else
        *tmp++ = '-';

    if(trace_state == TRACE_DISABLE)
    {
        if(status & PP_FLAG)
            *tmp++ = 'P';
        else
            *tmp++ = '-';
        if(status & TIMEOUT_FLAG)
            *tmp++ = 'T';
        else
            *tmp++ = '-';
        if(status & BUS_ERROR_FLAG)
            *tmp++ = 'B';
        else
            *tmp++ = '-';
    }
    else
    {
// not used when tracing
        *tmp++ = '-';
        *tmp++ = '-';
        *tmp++ = '-';
    }
    *tmp = 0;

    if(trace_state == TRACE_READ || trace_state == TRACE_BUS)
    {
        if(status & DAV_FLAG)
            strcat(str,"  DAV");
        else
            strcat(str,"     ");
        if(status & NRFD_FLAG)
            strcat(str," NRFD");
        else
            strcat(str,"     ");
        if(status & NDAC_FLAG)
            strcat(str," NDAC");
        else
            strcat(str,"     ");
    }

    if( (status & ATN_FLAG) )
    {
        int i;
        int cmd = status & CMD_MASK;
        if(cmd >= 0x020 && cmd <= 0x3e)
            sprintf(tmp," MLA %02Xh", cmd & 0x1f);
        else if(cmd >= 0x040 && cmd <= 0x4e)
            sprintf(tmp," MTA %02Xh", cmd & 0x1f);
        else if(cmd >= 0x060 && cmd <= 0x6f)
            sprintf(tmp," MSA %02Xh", cmd & 0x1f);
        else
        {
            for(i=0;gpib_tokens[i].cmd != -1;++i)
            {
                if(cmd == gpib_tokens[i].cmd)
                {
                    strcat(tmp," ");
                    strcat(tmp,gpib_tokens[i].name);
                    break;
                }
            }
        }
    }

    if(gpib_log_fp == NULL)
        gpib_log_fp = stdout;

// Echo to console unless file is the console
    if(gpib_log_fp != stdout)
        puts(str);

// Save to file
    fprintf(gpib_log_fp,"%s\n",str);
}


/// @brief  Calls gpib_decode_str() and dosplays the result.
///
/// Display: decode/display all control flags and data on GPIB BUS.
/// @see gpib_decode_str()
/// @return  void

void gpib_decode(uint16_t ch)
{
    gpib_trace_display(ch,0);
}


/// @brief  Read string from GPIB BUS - controlled by status flags.
///
/// - Status flags used when reading
///   - If EOI is set then EOI detection will end reading when EOI is detected.
///   - If an early or unexpected EOI is detected a warning is displayed.
///   - The state of ATN controls the type of data read.
///     - If ATN is set then parity is stripped.
///     - Only data matching the ATN flag state set by user is read.
///       - If we see a mismatch we "unread" it and exit.
///       - Unreading saves data AND status to be reread later on.
///
/// @see: gpib_read_byte()
/// @see: gpib_unread()
///
/// @param[in] buf: Binary gpib string to read
/// @param[in] size: Size of string we want to read
/// @param[in] status: controls sending modes and returns status
///
/// @return bytes read
/// @return status
///
/// - Errors TIMEOUT_FLAG or IFC_FLAG will cause early exit and set status.
///   - Lower 8 bits: Data or Command.
///     - If ATN is LOW then we strip parity from the byte.
///   - Upper 8 bits: Status and Errors.
///     - @see gpib.h _FLAGS defines for a full list.
///     - An error implies the data byte can't be trusted
///     - Control Line Flags.
///       - EOI_FLAG
///       - SRQ_FLAG
///       - ATN_FLAG
///       - REN_FLAG
///       - PP_FLAG
///     - Error Flags:
///       - IFC_FLAG
int gpib_read_str(uint8_t *buf, int size, uint16_t *status)
{
    uint16_t val;
    int ind = 0;

    *status &= STATUS_MASK;

    if(!size)
    {
        if(debuglevel & (GPIB_ERR + GPIB_DEVICE_STATE_MESSAGES + GPIB_RW_STR_BUS_DECODE))
            printf("gpib_read_str: size = 0\n");
    }

    while(ind < size)
    {
        val = gpib_read_byte(NO_TRACE);
#if SDEBUG
        if(debuglevel & GPIB_RW_STR_BUS_DECODE)
            gpib_decode(val);
#endif
        if(val & ERROR_MASK)
        {
            *status |= (val & ERROR_MASK);
            break;
        }

        if((*status & ATN_FLAG) != (val & ATN_FLAG))
        {
            if(debuglevel & (GPIB_ERR + GPIB_DEVICE_STATE_MESSAGES + GPIB_RW_STR_BUS_DECODE))
                printf("gpib_read_str(ind:%d): ATN %02XH unexpected\n",ind, 0xff & val);
            gpib_unread(val);
            break;
        }

        if(val & ATN_FLAG)
            buf[ind] = (val & CMD_MASK);
        else
            buf[ind] = (val & DATA_MASK);
        ++ind;

        if(!(val & ATN_FLAG) && (val & EOI_FLAG) )
        {

            if(*status & EOI_FLAG)
                return(ind);
/// @todo TODO
///  decode this state - for now I just set the EOI_FLAG
            *status |= EOI_FLAG;
            break;
        }
    }
    if ( ind != size ) 
    {
        if(debuglevel & (GPIB_ERR + GPIB_DEVICE_STATE_MESSAGES))
            printf("[gpib_read_str read(%d) expected(%d)]\n", ind , size);
    }
    return(ind);
}


/// @brief  Send string to GPIB BUS - controlled by status flags.
///
/// - Status flags used when sending
///   - If EOI is set then EOI is sent at last character
///
/// @param[in] buf: Binary gpib string to send
/// @param[in] size: Size of string
/// @param[in] status: User status flags that control the sending process.
///
/// @return bytes sent
///  - will match size on success (no other tests needed).
///  - Any size mismatch impiles error flags IFC_FLAG and TIMEOUT_FLAG.
///
/// - Errors TIMEOUT_FLAG or IFC_FLAG will cause early exit and set status.
///   - Lower 8 bits: Data or Command.
///     - If ATN is LOW then we strip parity from the byte.
///   - Upper 8 bits: Status and Errors.
///     - @see gpib.h _FLAGS defines for a full list.
///     - An error implies the data byte can't be trusted
///     - Control Line Flags.
///       - EOI_FLAG
///       - SRQ_FLAG
///       - ATN_FLAG
///       - REN_FLAG
///       - PP_FLAG
///     - Error Flags:
///       - IFC_FLAG
/// @see: gpib_write_byte()
/// @see: gpib.h _FLAGS defines for a full list)
int gpib_write_str(uint8_t *buf, int size, uint16_t *status)
{
    uint16_t val, ch;
    int ind = 0;

    *status &= STATUS_MASK;

    if(!size)
    {
        if(debuglevel & (GPIB_ERR + GPIB_DEVICE_STATE_MESSAGES + GPIB_RW_STR_BUS_DECODE))
            printf("gpib_write_str: size = 0\n");
    }

	// Start with NRFD and NDAC = 1 - ie off the OC BUS
	gpib_rx_init(0);

// Wait until ATN is released!
#if 1
    if (GPIB_PIN_TST(ATN) == 0)
	{
		// Switch to READ mode and NRFD = 0, NDAC = 0
		// gpib_rx_init(0);
#if 0
		// This debug statements only proves ATN can be low when called
		printf("gpib_write_str: ATN = 1 at start\n");
#endif
		// Wait for ATN free
		// Keep in mind that we have been addressed to talk already
		// So waiting is ok - they won't be expecting a reply until
		// They are ready
		gpib_timeout_set(HTIMEOUT);
		while(GPIB_PIN_TST(ATN) == 0)
		{
			if(gpib_timeout_test())
			{
				gpib_rx_init(1);
				if(debuglevel & (GPIB_ERR + GPIB_BUS_OR_CMD_BYTE_MESSAGES))
					printf("<gpib_write_str timeout waiting for ATN = 1>\n");
				*status |= (TIMEOUT_FLAG | BUS_ERROR_FLAG);
				return(ind);
			}
		}
	}
#endif

// Wait until DAV is released!
#if 1
	// Wait if DAV = 0 as the bus is busy
    gpib_timeout_set(HTIMEOUT);
	while ( GPIB_PIN_TST(DAV) == 0)
	{
		if(gpib_timeout_test())
		{
			if(debuglevel & (GPIB_ERR + GPIB_BUS_OR_CMD_BYTE_MESSAGES))
				printf("<BUS waiting for DAV==1>\n");
			*status |= (TIMEOUT_FLAG | BUS_ERROR_FLAG);
			return(ind);
		}
	}
#endif

    while(ind < size)
    {
        ch = buf[ind++] & 0xff;                   // unsigned

        if( (*status & EOI_FLAG) && (ind == size ) )
            ch |= EOI_FLAG;

/// @return Returns

        val = gpib_write_byte(ch);
        *status |= (val & ERROR_MASK);

#if SDEBUG
        if(debuglevel & GPIB_RW_STR_BUS_DECODE)
            gpib_decode(val);
#endif
        if(val & ERROR_MASK)
        {
            break;
        }

    }                                             // while(ind < size)

// End by setting receive mode and set NRFD and NDAC busy until
// we get back to the main loop (this happens very quickly
	gpib_rx_init(1);	// BUSY

    if ( ind != size )
    {
        if(debuglevel & (GPIB_ERR + GPIB_DEVICE_STATE_MESSAGES + GPIB_RW_STR_BUS_DECODE))
            printf("[gpib_write_str sent(%d) expected(%d)]\n", ind,size);
    }
    return(ind);
}
