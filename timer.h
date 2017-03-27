#ifndef TIMER_H_INCLUDED
#define TIMER_H_INCLUDED

#include <e_ctimers.h>

/// see also e_mesh_transmitter.c in epiphany-examples/emesh/emesh_traffic/src
#define E_CTIMER_MESH_0 (0xf)
#define E_CTIMER_MESH_1 (0xe)

#define E_MESHEVENT_ANYWAIT1                0x00000200
#define E_MESHEVENT_COREWAIT1           0x00000300
#define E_MESHEVENT_SOUTHWAIT1          0x00000400
#define E_MESHEVENT_NORHTWAIT1          0x00000500
#define E_MESHEVENT_WESTWAIT1           0x00000600
#define E_MESHEVENT_EASTWAIT1           0x00000700
#define E_MESHEVENT_SOUTHEASTWAIT1      0x00000800
#define E_MESHEVENT_CNORTHWESTWAIT1     0x00000900
#define E_MESHEVENT_SOUTHACCESS1        0x00000A00
#define E_MESHEVENT_NORTHACCESS1        0x00000B00
#define E_MESHEVENT_WESTACCESS1         0x00000C00
#define E_MESHEVENT_EASTACCESS1         0x00000D00
#define E_MESHEVENT_COREACCESS1         0x00000E00
#define E_MESHEVENT_ANYACCESS1          0x00000F00 /// Epiphany IV only


//#define PREPAREMESHTIMER1(mesh_reg, event_type) mesh_reg = e_reg_read(E_REG_MESHCFG);       int mesh_reg_timer = mesh_reg & 0xfffff0ff;	                mesh_reg_timer = mesh_reg_timer | event_type;	e_reg_write(E_REG_MESHCFG, mesh_reg_timer);
#define PREPAREMESHTIMER1(mesh_reg, event_type) mesh_reg = e_reg_read(E_REG_MESHCFG);       e_reg_write(E_REG_MESHCFG, ((mesh_reg & 0xfffff0ff) | event_type));     /// better
#define STARTMESHTIMER1(start_ticks)            e_ctimer_set(E_CTIMER_1, E_CTIMER_MAX) ;    start_ticks = e_ctimer_start(E_CTIMER_1, E_CTIMER_MESH_1);
#define STOPMESHTIMER1(stop_ticks)              stop_ticks = e_ctimer_get(E_CTIMER_1);      e_ctimer_stop(E_CTIMER_1);
#define RESETMESHTIMER1(mesh_reg)               e_reg_write(E_REG_MESHCFG, mesh_reg);


#define STARTCLOCK0(start_ticks)         e_ctimer_set(E_CTIMER_0, E_CTIMER_MAX);   start_ticks = e_ctimer_start(E_CTIMER_0, E_CTIMER_CLK);
#define STOPCLOCK0(stop_ticks)           stop_ticks = e_ctimer_get(E_CTIMER_0);    e_ctimer_stop(E_CTIMER_0);

#define STARTIDLECOUNTER1(start_ticks)   e_ctimer_set(E_CTIMER_1, E_CTIMER_MAX);   start_ticks = e_ctimer_start(E_CTIMER_1, E_CTIMER_IDLE);
#define STOPIDLECOUNTER1(stop_ticks)     stop_ticks = e_ctimer_get(E_CTIMER_1);    e_ctimer_stop(E_CTIMER_1);
#define STARTCLOCK1(start_ticks)         e_ctimer_set(E_CTIMER_1, E_CTIMER_MAX);   start_ticks = e_ctimer_start(E_CTIMER_1, E_CTIMER_CLK);
#define STOPCLOCK1(stop_ticks)           stop_ticks = e_ctimer_get(E_CTIMER_1);    e_ctimer_stop(E_CTIMER_1);


#endif // TIMER_H_INCLUDED
