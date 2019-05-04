#ifdef MC_66B
#	define HW_MODEL_STRING "MC-B"
#elif defined(EN61107)
#	define HW_MODEL_STRING "MC"
#elif defined(IMPULSE)
#	define HW_MODEL_STRING "IMPULSE"
#elif defined(DEBUG_NO_METER)
#	define HW_MODEL_STRING "NO_METER"
#else
#	define HW_MODEL_STRING "KMP"
#endif

#ifdef FLOW_METER
#	define FLOW_METER_STRING "-FLOW"
#else
#	define FLOW_METER_STRING ""
#endif	// FLOW_METER

#if defined(IMPULSE) || defined(DEBUG_NO_METER)
#	define THERMO_TYPE_STRING ""
#else
#	ifdef THERMO_NO
#		define THERMO_TYPE_STRING "-THERMO_NO"
#	else	// THERMO_NC
#		define THERMO_TYPE_STRING "-THERMO_NC"
#	endif	// THERMO_NO
#endif	// // not IMPULSE or DEBUG_NO_METER

#if defined(IMPULSE) || defined(DEBUG_NO_METER)
#	define THERMO_ON_AC_2_STRING ""
#else
#	ifdef THERMO_ON_AC_2
#		define THERMO_ON_AC_2_STRING "-THERMO_ON_AC_2"
#	else
#		define THERMO_ON_AC_2_STRING ""
#	endif	// THERMO_ON_AC_2
#endif	// // not IMPULSE or DEBUG_NO_METER

#ifdef AUTO_CLOSE
#	define AUTO_CLOSE_STRING ""
#else
#	define AUTO_CLOSE_STRING "-NO_AUTO_CLOSE"
#endif	// AUTO_CLOSE

#ifdef DEBUG_STACK_TRACE
#	define DEBUG_STACK_TRACE_STRING "-DEBUG_STACK_TRACE"
#else
#	define DEBUG_STACK_TRACE_STRING ""
#endif	// DEBUG_STACK_TRACE
	
#ifdef NO_CRON
#	define NO_CRON_STRING "-NO_CRON"
#else
#	define NO_CRON_STRING ""
#endif	// DEBUG_STACK_TRACE
	
#define HW_MODEL (HW_MODEL_STRING FLOW_METER_STRING THERMO_TYPE_STRING THERMO_ON_AC_2_STRING AUTO_CLOSE_STRING DEBUG_STACK_TRACE_STRING NO_CRON_STRING)
