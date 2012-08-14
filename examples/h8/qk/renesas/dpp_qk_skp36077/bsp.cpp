//////////////////////////////////////////////////////////////////////////////
// Product: DPP example, H8/SKP36077 board, QK
// Last Updated for Version: 4.0.01
// Date of the Last Update:  Oct 21, 2008
//
//                    Q u a n t u m     L e a P s
//                    ---------------------------
//                    innovating embedded systems
//
// Copyright (C) 2002-2008 Quantum Leaps, LLC. All rights reserved.
//
// This software may be distributed and modified under the terms of the GNU
// General Public License version 2 (GPL) as published by the Free Software
// Foundation and appearing in the file GPL.TXT included in the packaging of
// this file. Please note that GPL Section 2[b] requires that all works based
// on this software must also be made publicly available under the terms of
// the GPL ("Copyleft").
//
// Alternatively, this software may be distributed and modified under the
// terms of Quantum Leaps commercial licenses, which expressly supersede
// the GPL and are specifically designed for licensees interested in
// retaining the proprietary status of their code.
//
// Contact information:
// Quantum Leaps Web site:  http://www.quantum-leaps.com
// e-mail:                  info@quantum-leaps.com
//////////////////////////////////////////////////////////////////////////////
#include "qp_port.h"
#include "dpp.h"
#include "bsp.h"

#include "skp36077def.h"                         // SKP36077 board definitions

Q_DEFINE_THIS_FILE

// Local objects -------------------------------------------------------------
#ifdef Q_SPY
    #define BAUD_RATE 38400

    #pragma section QS
    static uint8_t l_qsBuf[0x200];                            // buffer for QS
    #pragma section

    static uint32_t l_tickTime;

    enum AppRecords {                    // application-specific trace records
        PHILO_STAT = QS_USER
    };
#endif


//............................................................................
__interrupt(vect = 26)
void timer_z_isr(void) {
    QK_ISR_ENTRY();                         // inform QK about entering an ISR

    TZ0.TSR.BIT.IMFA = 0;                          // clear compare match flag
#ifdef Q_SPY
    l_tickTime += (uint32_t)((f1_CLK_SPEED/8 + BSP_TICKS_PER_SEC/2)
                             / BSP_TICKS_PER_SEC);
#endif
    QF::tick();                               // process all armed time events

    QK_ISR_EXIT();                           // inform QK about exiting an ISR
}
//............................................................................
void BSP_init(void) {
    WDT.TCSRWD.BYTE = 0x10;                                // disable Watchdog
    WDT.TCSRWD.BYTE = 0x00;

    LED1_DDR_1();                              // configure LED1 pin as output
    LED2_DDR_1();                              // configure LED2 pin as output
    LED1 = LED_OFF;
    LED2 = LED_OFF;

    MSTCR2.BIT.MSTTZ   = 0;                                  // turn on TimerZ
    TZ0.TCR.BIT.TPSC   = 3;                            // internal clock phi/8
    TZ0.TCR.BIT.CCLR   = 1;
    TZ0.GRA            = (uint16_t)(((f1_CLK_SPEED/8 + BSP_TICKS_PER_SEC/2)
                                     / BSP_TICKS_PER_SEC) - 1);
    TZ0.TIER.BIT.IMIEA = 1;                  // compare match interrupt enable

    if (QS_INIT((void *)0) == 0) {       // initialize the QS software tracing
        Q_ERROR();
    }
}
//............................................................................
void QF::onStartup(void) {
    TZ.TSTR.BIT.STR0 = 1;                                      // start TimerZ
}
//............................................................................
void QF::onCleanup(void) {
}
//............................................................................
void QK_init(void) {
}
//............................................................................
void QK::onIdle(void) {
#ifdef Q_SPY
    if ((SCI3.SSR.BYTE & 0x80) != 0) {                      // TDRE not ready?
        QF_INT_DISABLE();                                // disable interrupts
        uint16_t b = QS::getByte();
        QF_INT_ENABLE();                                  // enable interrupts
        if (b != QS_EOD) {
            SCI3.TDR = (uint8_t)b;     // put the byte to the TX data register
        }
    }
#elif (defined NDEBUG)             // low-power mode interferes with debugging
    // stop all peripheral clocks that you can in your applicaiton ...
    sleep();             //  before entering any pending interrupt, see NOTE01
#endif
}
//............................................................................
void BSP_displyPhilStat(uint8_t n, char const *stat) {
    switch (n) {
        case 0: LED1 = (stat[0] == (uint8_t)'e') ? LED_ON : LED_OFF; break;
        case 1: LED2 = (stat[0] == (uint8_t)'e') ? LED_ON : LED_OFF; break;
    }

    QS_BEGIN(PHILO_STAT, AO_Philo[n])     // application-specific record begin
        QS_U8(1, n);                                     // Philosopher number
        QS_STR(stat);                                    // Philosopher status
    QS_END()
}
//----------------------------------------------------------------------------
void Q_onAssert(char const Q_ROM * const Q_ROM_VAR file, int line) {
    QF_INT_DISABLE();                                    // disable interrupts
    for (;;) {                                   // hang in this for-ever loop
    }
}

//----------------------------------------------------------------------------
#ifdef Q_SPY
uint8_t QS::onStartup(void const *arg) {
    initBuf(l_qsBuf, sizeof(l_qsBuf));

    IO.PMR1.BIT.TXD  = 1;                             // enable SCI3 TX output

    MSTCR1.BIT.MSTS3 = 0x0;          // clear the SCI3 module stop control bit
    SCI3.SCR3.BYTE   = 0x00;                 // clear TE & RE bits to 0 in SCR
    SCI3.SSR.BYTE   &= ~(0x8 | 0x10 | 0x20);              // clear error flags
    SCI3.SMR.BYTE    = 0x00;          // set 8-bit data, 1-stop bit, no parity
    SCI3.BRR         = (uint8_t)((f1_CLK_SPEED + 32*BAUD_RATE/2)
                                 / (32*BAUD_RATE) - 1);
    SCI3.SCR3.BYTE  |= 0x20;                            // enable transmission

                                                    // setup the QS filters...
    QS_FILTER_ON(QS_ALL_RECORDS);

//    QS_FILTER_OFF(QS_QEP_STATE_EMPTY);
//    QS_FILTER_OFF(QS_QEP_STATE_ENTRY);
//    QS_FILTER_OFF(QS_QEP_STATE_EXIT);
//    QS_FILTER_OFF(QS_QEP_STATE_INIT);
//    QS_FILTER_OFF(QS_QEP_INIT_TRAN);
//    QS_FILTER_OFF(QS_QEP_INTERN_TRAN);
//    QS_FILTER_OFF(QS_QEP_TRAN);
//    QS_FILTER_OFF(QS_QEP_dummyD);

    QS_FILTER_OFF(QS_QF_ACTIVE_ADD);
    QS_FILTER_OFF(QS_QF_ACTIVE_REMOVE);
    QS_FILTER_OFF(QS_QF_ACTIVE_SUBSCRIBE);
    QS_FILTER_OFF(QS_QF_ACTIVE_UNSUBSCRIBE);
    QS_FILTER_OFF(QS_QF_ACTIVE_POST_FIFO);
    QS_FILTER_OFF(QS_QF_ACTIVE_POST_LIFO);
    QS_FILTER_OFF(QS_QF_ACTIVE_GET);
    QS_FILTER_OFF(QS_QF_ACTIVE_GET_LAST);
    QS_FILTER_OFF(QS_QF_EQUEUE_INIT);
    QS_FILTER_OFF(QS_QF_EQUEUE_POST_FIFO);
    QS_FILTER_OFF(QS_QF_EQUEUE_POST_LIFO);
    QS_FILTER_OFF(QS_QF_EQUEUE_GET);
    QS_FILTER_OFF(QS_QF_EQUEUE_GET_LAST);
    QS_FILTER_OFF(QS_QF_MPOOL_INIT);
    QS_FILTER_OFF(QS_QF_MPOOL_GET);
    QS_FILTER_OFF(QS_QF_MPOOL_PUT);
    QS_FILTER_OFF(QS_QF_PUBLISH);
    QS_FILTER_OFF(QS_QF_NEW);
    QS_FILTER_OFF(QS_QF_GC_ATTEMPT);
    QS_FILTER_OFF(QS_QF_GC);
//    QS_FILTER_OFF(QS_QF_TICK);
    QS_FILTER_OFF(QS_QF_TIMEEVT_ARM);
    QS_FILTER_OFF(QS_QF_TIMEEVT_AUTO_DISARM);
    QS_FILTER_OFF(QS_QF_TIMEEVT_DISARM_ATTEMPT);
    QS_FILTER_OFF(QS_QF_TIMEEVT_DISARM);
    QS_FILTER_OFF(QS_QF_TIMEEVT_REARM);
    QS_FILTER_OFF(QS_QF_TIMEEVT_POST);
    QS_FILTER_OFF(QS_QF_INT_LOCK);
    QS_FILTER_OFF(QS_QF_INT_UNLOCK);
    QS_FILTER_OFF(QS_QF_ISR_ENTRY);
    QS_FILTER_OFF(QS_QF_ISR_EXIT);

    return (uint8_t)1;               // indicate successfull QS initialization
}
//............................................................................
void QS::onCleanup(void) {
}
//............................................................................
void QS::onFlush(void) {
    uint16_t b;
    while ((b = getByte()) != QS_EOD) {       // next QS trace byte available?
        while ((SCI3.SSR.BYTE & 0x80) == 0) {               // TDRE not ready?
        }
        SCI3.TDR = (uint8_t)b;       // stick the byte to the TX data register
    }
}
//............................................................................
// NOTE: invoked within a critical section (inetrrupts disabled)
QSTimeCtr QS::onGetTime(void) {
    if (TZ0.TSR.BIT.IMFA == 0) {          // input compare match flag NOT set?
        return l_tickTime + (uint32_t)TZ0.TCNT;
    }
    else {          // the output compare occured, but the ISR did not run yet
        return l_tickTime
               + (uint32_t)((f1_CLK_SPEED/8 + BSP_TICKS_PER_SEC/2)
                            / BSP_TICKS_PER_SEC)
               + (uint32_t)TZ0.TCNT;
    }
}
#endif                                                                // Q_SPY
//----------------------------------------------------------------------------

//////////////////////////////////////////////////////////////////////////////
// NOTE01:
// The non-preemptive QF port (foreground/background) requires an atomic
// transiton to the low-power WAIT mode of the M16C MCU. As described in
// "M16C Software Manual", Section 5.2.1 "Interrupt Enable Flag", the
// instruction immediately following "FSET I" instruction, will be executed
// before any pending interrupt. This guarantees atomic transition to the
// WAIT mode. CAUTION: The instruction pair (FSET I, WAIT) should never be
// separated by any other instruction.
//