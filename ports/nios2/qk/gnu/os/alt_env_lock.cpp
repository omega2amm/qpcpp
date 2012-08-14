//////////////////////////////////////////////////////////////////////////////
// Product: QK/C++ port to Altera Nios II, QK preemptive kernel
// Last Updated for Version: 4.1.02
// Date of the Last Update:  Feb 11, 2010
//
//                    Q u a n t u m     L e a P s
//                    ---------------------------
//                    innovating embedded systems
//
// Copyright (C) 2002-2010 Quantum Leaps, LLC. All rights reserved.
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
#include "qk_pkg.h"
#include <reent.h>

Q_DEFINE_THIS_MODULE(alt_env_lock)

alt_u8 ALT_env_ceiling = QF_MAX_ACTIVE + 1;        // priority ceiling for env

static alt_u8 l_mutexStack[8];
static alt_u8 l_mutexStackTop = (alt_u8)0;

//.............................................................................
extern "C" void __env_lock(struct _reent *_r) {
    alt_irq_context ctx = alt_irq_disable_all();

    Q_ASSERT(l_mutexStackTop < sizeof(l_mutexStack));      // no stack overflow

    l_mutexStack[l_mutexStackTop++] = QK_ceilingPrio_;
                           // lock the QK scheduler up to the specified ceiling
    if (QK_ceilingPrio_ < ALT_env_ceiling) {
        QK_ceilingPrio_ = ALT_env_ceiling;
    }

    alt_irq_enable_all(ctx);
}
//.............................................................................
extern "C" void __env_unlock(struct _reent *_r) {
    alt_u8 mutex;
    alt_irq_context ctx = alt_irq_disable_all();

    Q_ASSERT(0 < l_mutexStackTop);               // the stack must not be empty

    mutex = l_mutexStack[--l_mutexStackTop];
    if (QK_ceilingPrio_ > mutex) {
        QK_ceilingPrio_ = mutex;          // restore the saved priority ceiling
        QK_schedule_(ctx);
    }
    alt_irq_enable_all(ctx);
}

