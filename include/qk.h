/// @file
/// @brief QK/C++ platform-independent public interface.
/// @ingroup qk
/// @cond
///***************************************************************************
/// Last updated for version 5.7.0
/// Last updated on  2016-08-21
///
///                    Q u a n t u m     L e a P s
///                    ---------------------------
///                    innovating embedded systems
///
/// Copyright (C) Quantum Leaps, LLC. All rights reserved.
///
/// This program is open source software: you can redistribute it and/or
/// modify it under the terms of the GNU General Public License as published
/// by the Free Software Foundation, either version 3 of the License, or
/// (at your option) any later version.
///
/// Alternatively, this program may be distributed and modified under the
/// terms of Quantum Leaps commercial licenses, which expressly supersede
/// the GNU General Public License and are specifically designed for
/// licensees interested in retaining the proprietary status of their code.
///
/// This program is distributed in the hope that it will be useful,
/// but WITHOUT ANY WARRANTY; without even the implied warranty of
/// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
/// GNU General Public License for more details.
///
/// You should have received a copy of the GNU General Public License
/// along with this program. If not, see <http://www.gnu.org/licenses/>.
///
/// Contact information:
/// http://www.state-machine.com
/// mailto:info@state-machine.com
///***************************************************************************
/// @endcond

#ifndef qk_h
#define qk_h

#include "qequeue.h" // QK kernel uses the native QF event queue
#include "qmpool.h"  // QK kernel uses the native QF memory pool
#include "qpset.h"   // QK kernel uses the native QF priority set


//****************************************************************************
// QF configuration for QK

//! This macro defines the type of the event queue used for active objects.
/// @description
///QK uses the native QF event queue QEQueue.
#define QF_EQUEUE_TYPE         QEQueue

//****************************************************************************
//! attributes of the QK kernel (in C for easy access in assembly)
extern "C" {

struct QK_Attr {
    uint_fast8_t volatile curr;  //!< priority of the current executing AO
    uint_fast8_t volatile next;  //!< priority of the next AO to execute
    void volatile        *aux;   //!< auxiliary attribute used in the port
    uint_fast8_t volatile lockPrio;   //!< lock prio (0 == no-lock)
    uint_fast8_t volatile lockHolder; //!< prio of the lock holder
#ifndef QK_ISR_CONTEXT_
    uint_fast8_t volatile intNest;    //!< ISR nesting level
#endif // QK_ISR_CONTEXT_
#if (QF_MAX_ACTIVE <= 8)
    QP::QPSet8  readySet;     //!< QK ready-set of AOs and "naked" threads
#else
    QP::QPSet64 readySet;     //!< QK ready-set of AOs and "naked" threads
#endif
};

//! global attributes of the QK kernel
extern QK_Attr QK_attr_;

//! QK initialization
void QK_init(void);

//! QK scheduler
void QK_sched_(uint_fast8_t p);

//! Find the highest-priority task ready to run
uint_fast8_t QK_schedPrio_(void);

} // extern "C"



//****************************************************************************
namespace QP {

//****************************************************************************
//! QK services.
/// @description
/// This class groups together QK services. It has only static members and
/// should not be instantiated.
///
// @note The QK scheduler, QK priority, QK ready set, etc. belong conceptually
/// to the QK class (as static class members). However, to avoid C++ potential
/// name-mangling problems in assembly language, these elements are defined
/// outside of the QK class and use the extern "C" linkage specification.
class QK {
public:

    //! get the current QK version number string of the form X.Y.Z
    static char_t const *getVersion(void) {
        return versionStr;
    }

    //! QK idle callback (customized in BSPs for QK)
    /// @description
    /// QP::QK::onIdle() is called continously by the QK idle loop. This
    /// callback gives the application an opportunity to enter a power-saving
    /// CPU mode, or perform some other idle processing.
    ///
    /// @note QP::QK::onIdle() is invoked with interrupts enabled and must
    /// also return with interrupts enabled.
    ///
    /// @sa QP::QF::onIdle()
    static void onIdle(void);
};

/*! Priority-ceiling Mutex the QK preemptive kernel */
class QMutex {
public:
    void init(uint_fast8_t const prio);
    void lock(void);
    void unlock(void);

private:
    uint_fast8_t m_lockPrio; //!< lock prio (priority ceiling)
    uint_fast8_t m_prevPrio; //!< previoius lock prio

    friend class QF;
};

} // namespace QP


//****************************************************************************
// interface used only inside QF, but not in applications

#ifdef QP_IMPL
    #ifndef QK_ISR_CONTEXT_
        //! Internal port-specific macro that reports the execution context
        // (ISR vs. thread).
        /// @returns true if the code executes in the ISR context and false
        /// otherwise
        #define QK_ISR_CONTEXT_() \
            (QK_attr_.intNest != static_cast<uint_fast8_t>(0))
    #endif // QK_ISR_CONTEXT_

    // QF-specific scheduler locking
    //! Internal port-specific macro to represent the scheduler lock status
    // that needs to be preserved to allow nesting of locks.
    #define QF_SCHED_STAT_TYPE_ QMutex

    //! Internal port-specific macro for selective scheduler locking.
    #define QF_SCHED_LOCK_(pLockStat_, prio_) do { \
        if (QK_ISR_CONTEXT_()) { \
            (pLockStat_)->m_lockPrio = \
                static_cast<uint_fast8_t>(QF_MAX_ACTIVE + 1); \
        } else { \
            (pLockStat_)->init((prio_)); \
            (pLockStat_)->lock(); \
        } \
    } while (false)

    //! Internal port-specific macro for selective scheduler unlocking.
    #define QF_SCHED_UNLOCK_(pLockStat_) (pLockStat_)->unlock()

    // native event queue operations...
    #define QACTIVE_EQUEUE_WAIT_(me_) \
        Q_ASSERT_ID(0, (me_)->m_eQueue.m_frontEvt != static_cast<QEvt *>(0))

    #define QACTIVE_EQUEUE_SIGNAL_(me_) do { \
        QK_attr_.readySet.insert((me_)->m_prio); \
        if (!QK_ISR_CONTEXT_()) { \
            uint_fast8_t p = QK_schedPrio_(); \
            if (p != static_cast<uint_fast8_t>(0)) { \
                QK_sched_(p); \
            } \
        } \
    } while (false)
    #define QACTIVE_EQUEUE_ONEMPTY_(me_) \
        QK_attr_.readySet.remove((me_)->m_prio)

    // native QF event pool operations...
    #define QF_EPOOL_TYPE_  QMPool
    #define QF_EPOOL_INIT_(p_, poolSto_, poolSize_, evtSize_) \
        (p_).init((poolSto_), (poolSize_), (evtSize_))
    #define QF_EPOOL_EVENT_SIZE_(p_) \
        static_cast<uint_fast16_t>((p_).getBlockSize())
    #define QF_EPOOL_GET_(p_, e_, m_) \
        ((e_) = static_cast<QEvt *>((p_).get((m_))))
    #define QF_EPOOL_PUT_(p_, e_) ((p_).put(e_))

#endif // QP_IMPL

#endif // qk_h
