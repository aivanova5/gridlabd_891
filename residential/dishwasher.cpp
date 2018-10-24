/** $Id: dishwasher.cpp 4738 2014-07-03 00:55:39Z dchassin $
	Copyright (C) 2008 Battelle Memorial Institute
	@file dishwasher.cpp
	@addtogroup dishwasher
	@ingroup residential


 @{
 **/
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <math.h>

#include "house_e.h"
#include "dishwasher.h"

EXPORT_CREATE(dishwasher)
EXPORT_INIT(dishwasher)
EXPORT_PRECOMMIT(dishwasher)
EXPORT_SYNC(dishwasher)
EXPORT_ISA(dishwasher)

extern "C" void dishwasher_decrement_queue(statemachine *machine)
{
	gld_object *obj = get_object(machine->context);
	gld_property queue(obj,"state_queue");
	double value = queue.get_double();
	queue.setp(value-1);
}
extern "C" void dishwasher_fill_unit(statemachine *machine)
{
	gld_object *obj = get_object(machine->context);
	gld_property gpm(obj,"hotwater_demand");
	gpm.setp(5.0);
}
extern "C" void dishwasher_check_fill(statemachine *machine)
{
	gld_object *obj = get_object(machine->context);
	gld_property gpm(obj,"hotwater_demand");

	// stop filling after 1 minute
	if ( gpm.get_double()>0 && machine->runtime>60 )
	{
		gld_object *obj = get_object(machine->context);
		gld_property gpm(obj,"hotwater_demand");
		gpm.setp(0.0);
	}
}
extern "C" void dishwasher_drain_unit(statemachine *machine)
{
	// TODO nothing here?
}
extern "C" void dishwasher_heat_water(statemachine *machine)
{
	// TODO turn on heater
}
extern "C" void dishwasher_check_water(statemachine *machine)
{
	// TODO turn off heater when water reaches setpoint
}
extern "C" void dishwasher_heat_air(statemachine *machine)
{
	// TODO turn on heater
}
extern "C" void dishwasher_check_air(statemachine *machine)
{
	// TODO turn off heater when water reaches setpoint
}

//////////////////////////////////////////////////////////////////////////
// dishwasher CLASS FUNCTIONS
//////////////////////////////////////////////////////////////////////////
CLASS* dishwasher::oclass = NULL;
CLASS* dishwasher::pclass = NULL;
dishwasher *dishwasher::defaults = NULL;

dishwasher::dishwasher(MODULE *module) : residential_enduse(module)
{
	// first time init
	if (oclass==NULL)
	{
		pclass = residential_enduse::oclass;
		
		// register the class definition
		oclass = gl_register_class(module,"dishwasher",sizeof(dishwasher),PC_BOTTOMUP|PC_AUTOLOCK);
		if (oclass==NULL)
			GL_THROW("unable to register object class implemented by %s",__FILE__);

		// publish the class properties
		if (gl_publish_variable(oclass,
			PT_INHERIT, "residential_enduse",
			PT_enumeration,"washmode",PADDR(washmode), PT_DESCRIPTION, "the washing mode selected",
				PT_KEYWORD, "QUICK", QUICK,
				PT_KEYWORD, "NORMAL", NORMAL,
				PT_KEYWORD, "HEAVY", HEAVY,
			PT_enumeration,"drymode",PADDR(drymode), PT_DESCRIPTION, "the drying mode selected",
				PT_KEYWORD, "AIR", AIR,
				PT_KEYWORD, "HEAT", HEAT,
			PT_enumeration, "controlmode", PADDR(controlmode), PT_DESCRIPTION, "the current state of the dishwasher",
				PT_KEYWORD, "OFF",				OFF,
				PT_KEYWORD, "CONTROLSTART",		CONTROLSTART,
				PT_KEYWORD, "PUMPPREWASH",		PUMPPREWASH,
				PT_KEYWORD, "PUMPWASHQUICK",	PUMPWASHQUICK,
				PT_KEYWORD, "HEATWASHQUICK",	HEATWASHQUICK,
				PT_KEYWORD, "HEATWASHNORMAL",	HEATWASHNORMAL,
				PT_KEYWORD, "PUMPWASHNORMAL",	PUMPWASHNORMAL,
				PT_KEYWORD, "HEATWASHHEAVY",	HEATWASHHEAVY,
				PT_KEYWORD, "PUMPWASHHEAVY",	PUMPWASHHEAVY,
				PT_KEYWORD, "CONTROLWASH",		CONTROLWASH,
				PT_KEYWORD, "PUMPRINSE",		PUMPRINSE,
				PT_KEYWORD,	"HEATRINSE",		HEATRINSE,
				PT_KEYWORD,	"CONTROLRINSE",		CONTROLRINSE,
				PT_KEYWORD, "HEATDRYON",		HEATDRYON,
				PT_KEYWORD, "HEATDRYOFF",		HEATDRYOFF,
				PT_KEYWORD, "CONTROLDRY",		CONTROLDRY,
				PT_KEYWORD, "CONTROLEND",		CONTROLEND,
			PT_double_array,"state_duration",PADDR(state_duration), PT_DESCRIPTION, "the duration of each state",
			PT_double_array,"state_power_Z_real",PADDR(state_power_Z_real), PT_DESCRIPTION, "the Z component of real power of each state", 
			PT_double_array,"state_power_I_real",PADDR(state_power_I_real), PT_DESCRIPTION, "the I component of real power of each state", 
			PT_double_array,"state_power_P_real",PADDR(state_power_P_real), PT_DESCRIPTION, "the P component of real power of each state", 
			PT_double_array,"state_power_Z_reactive",PADDR(state_power_Z_reactive), PT_DESCRIPTION, "the Z component of reactive power of each state", 
			PT_double_array,"state_power_I_reactive",PADDR(state_power_I_reactive), PT_DESCRIPTION, "the I component of reactive power of each state", 
			PT_double_array,"state_power_P_reactice",PADDR(state_power_P_reactive), PT_DESCRIPTION, "the P component of reactive power of each state", 
			PT_double_array,"state_heatgain",PADDR(state_heatgain), PT_DESCRIPTION, "the heat gain to the house air for each state",
			PT_statemachine,"state_machine",PADDR(state_machine), PT_DESCRIPTION, "the state machine",
			PT_double,"pump_power_Z_real[kW]",PADDR(pump_power_Z_real), PT_DESCRIPTION, "the real power Z component consumed by the pump when it runs [kW]",
			PT_double,"pump_power_I_real[kW]",PADDR(pump_power_I_real), PT_DESCRIPTION, "the real power I component consumed by the pump when it runs [kW]",
			PT_double,"pump_power_P_real[kW]",PADDR(pump_power_P_real), PT_DESCRIPTION, "the real power P component consumed by the pump when it runs [kW]",
			PT_double,"pump_power_Z_reactive[kVA]",PADDR(pump_power_Z_reactive), PT_DESCRIPTION, "the reactive power Z component consumed by the pump when it runs [kVA]",
			PT_double,"pump_power_I_reactive[kVA]",PADDR(pump_power_I_reactive), PT_DESCRIPTION, "the reactive power I component consumed by the pump when it runs [kVA]",
			PT_double,"pump_power_P_reactive[kVA]",PADDR(pump_power_P_reactive), PT_DESCRIPTION, "the reactive power P component consumed by the pump when it runs [kVA]",
			PT_double,"coil_power_wet_real",PADDR(coil_power_wet_real), PT_DESCRIPTION, "the power consumed by the coil when it is on and wet",
			PT_double,"coil_power_dry_real",PADDR(coil_power_dry_real), PT_DESCRIPTION, "the power consumed by the coil when it is on and dry",
			PT_double,"coil_power_wet_reactive",PADDR(coil_power_wet_reactive), PT_DESCRIPTION, "the power consumed by the coil when it is on and wet",
			PT_double,"coil_power_dry_reactive",PADDR(coil_power_dry_reactive), PT_DESCRIPTION, "the power consumed by the coil when it is on and dry",
			PT_double,"control_power_Z_real[kW]",PADDR(control_power_Z_real), PT_DESCRIPTION, "the real power Z component consumed by the controller when it is on",
			PT_double,"control_power_I_real[kW]",PADDR(control_power_I_real), PT_DESCRIPTION, "the real power I component consumed by the controller when it is on",
			PT_double,"control_power_P_real[kW]",PADDR(control_power_P_real), PT_DESCRIPTION, "the real power P component consumed by the controller when it is on",
			PT_double,"control_power_Z_reactive[kVA]",PADDR(control_power_Z_reactive), PT_DESCRIPTION, "the reactive power Z component consumed by the controller when it is on",
			PT_double,"control_power_I_reactive[kVA]",PADDR(control_power_I_reactive), PT_DESCRIPTION, "the reactive power I component consumed by the controller when it is on",
			PT_double,"control_power_P_reactive[kVA]",PADDR(control_power_P_reactive), PT_DESCRIPTION, "the reactive power P component consumed by the controller when it is on",
			PT_complex,"total_power", PADDR(total_power), PT_DESCRIPTION, "total power consumed by the appliance",
			PT_complex,"total_admittance", PADDR(total_admittance), PT_DESCRIPTION, "total power consumed by the appliance",
			PT_complex,"total_current", PADDR(total_current), PT_DESCRIPTION, "total power consumed by the appliance",
			PT_double,"demand_rate[unit/day]",PADDR(demand_rate), PT_DESCRIPTION, "the rate at which dishwasher loads accumulate per day",
			PT_double,"state_queue",PADDR(state_queue), PT_DESCRIPTION, "the accumulate demand (units)",
			PT_double,"hotwater_demand[gpm]", PADDR(hotwater_demand), PT_DESCRIPTION, "the rate at which hotwater is being consumed",
			PT_double,"hotwater_temperature[degF]", PADDR(hotwater_temperature), PT_DESCRIPTION, "the incoming hotwater temperature",
			PT_double,"hotwater_temperature_drop[degF]", PADDR(hotwater_temperature_drop), PT_DESCRIPTION, "the temperature drop from the plumbing bus to the dishwasher",
			PT_double,"energy_baseline[kWh]", PADDR(energy_baseline), PT_DESCRIPTION, "baseline energy consumed by the appliance",
			PT_bool,"heated_dry_enabled", PADDR(heated_dry_enabled), PT_DESCRIPTION, "the selection of ON/OFF of the Heated dry cycle",
			PT_double,"daily_dishwasher_demand[kWh]", PADDR(daily_dishwasher_demand), PT_DESCRIPTION, "amount of energy consumed by the dishwasher during the day",
			PT_double,"queue", PADDR(queue), PT_DESCRIPTION, "current queue value",
			PT_double,"queue_min", PADDR(queue_min), PT_DESCRIPTION, "minimum queue value",
			PT_double,"queue_max", PADDR(queue_max), PT_DESCRIPTION, "maximum queue value",
			NULL)<1)
			GL_THROW("unable to publish properties in %s",__FILE__);
	}
}

dishwasher::~dishwasher()
{
}

int dishwasher::create() 
{
	int res = residential_enduse::create();

	// name of enduse
	load.name = oclass->name;

	load.power = load.admittance = load.current = load.total = complex(0,0,J);
	load.voltage_factor = 1.0;
	load.power_factor = 1.0;
	load.power_fraction = 1;

	controlmode = OFF;
	washmode = NORMAL;
	drymode = HEAT;
	demand_rate = 1.0;
	state_duration.set_name("state_duration");
	state_power_Z_real.set_name("state_power_Z_real");
	state_power_I_real.set_name("state_power_I_real");
	state_power_P_real.set_name("state_power_P_real");
	state_power_Z_reactive.set_name("state_power_Z_reactive");
	state_power_I_reactive.set_name("state_power_I_reactive");
	state_power_P_reactive.set_name("state_power_P_reactive");
	state_heatgain.set_name("state_heatgain");

	gl_warning("explicit %s model is experimental and has not been validated", OBJECTHDR(this)->oclass->name);
	/* TROUBLESHOOT
		The dishwasher explicit model has some serious issues and should be considered for complete
		removal.  It is highly suggested that this model NOT be used.
	*/

	return res;
}

int dishwasher::init(OBJECT *parent)
{
	// @todo This class has serious problems and should be deleted and started from scratch. Fuller 9/27/2013.

	OBJECT *hdr = OBJECTHDR(this);
	if(parent != NULL){
		if((parent->flags & OF_INIT) != OF_INIT){
			char objname[256];
			gl_verbose("dishwasher::init(): deferring initialization on %s", gl_name(parent, objname, 255));
			return 2; // defer
		}
	}
	hdr->flags |= OF_SKIPSAFE; // TODO is this still needed

	load.power_factor = 1.0;
	load.breaker_amps = 30;

	// defaults - broken down into real and reactive power
	if ( pump_power_Z_real==0 ) pump_power_Z_real=0.02;
	if ( pump_power_I_real==0 ) pump_power_I_real=0.05;
	if ( pump_power_P_real==0 ) pump_power_P_real=0.05;
	if ( pump_power_Z_reactive==0 ) pump_power_Z_reactive=0.05;
	if ( coil_power_wet_real==0 ) coil_power_wet_real=0.950;
	if ( coil_power_dry_real==0 ) coil_power_dry_real=0.695;
	if ( control_power_Z_real==0 ) control_power_Z_real=0.01;

	//reactive component
	//if ( pump_power.Mag()==0 ) pump_power.SetRect(250,82);
	//if ( coil_power_wet.Mag()==0 ) coil_power_wet.SetRect(950,0);
	//if ( coil_power_dry.Mag()==0 ) coil_power_dry.SetRect(695,0);
	//if ( control_power.Mag()==0 ) control_power.SetRect(10,0);


	// count the number of states
	gld_property statelist = get_controlmode_property();
	n_states = 0;
	for ( gld_keyword *keyword=statelist.get_first_keyword() ; keyword!=NULL ; keyword=keyword->get_next() )
	{
		if ( keyword->get_value()>n_states )
			n_states = keyword->get_value();
	}
	n_states++;

	// set up the default duration array if not set
	if ( state_duration.get_rows()==0 )
	{
		state_duration.grow_to(n_states,1);
		double duration[] = {
			0, // OFF
			1, // CONTROLSTART
			2, // PUMPREWASH
			3, // PUMPWASHQUICK
			4, // HEATWASHQUICK
			5, // HEATWASHNORMAL
			6,	// PUMPWASHNORMAL
			7, // HEATWASHHEAVY
			8,	// PUMPWASHHEAVY
			9, // CONTROLWASH
			10, // PUMPRINSE
			11, // HEATRINSE
			12, // CONTROLRINSE
			13,	// HEATDRYON
			14, // HEATDRYOFF
			15, // CONTROLDRY
			16, // CONTROLEND
			
		};

		for ( size_t n=0 ; n<sizeof(duration)/sizeof(duration[0]) ; n++ )
			state_duration.set_at(n,0,duration[n]*60);
	}

	// check that the duration has the correct number of entries
	if ( state_duration.get_rows()!= n_states)
	{

		exception("state_duration array does not have the same number of rows as the controlmode enumeration has possible values");
	}	
	else
		state_machine.transition_holds = &state_duration;

	// setup default state power array
	if ( state_power_Z_real.get_rows()==0 && state_power_Z_real.get_cols()==0 )
	{
		state_power_Z_real.grow_to(n_states,1);
		state_power_I_real.grow_to(n_states,1);
		state_power_P_real.grow_to(n_states,1);
		state_power_Z_reactive.grow_to(n_states,1);
		state_power_I_reactive.grow_to(n_states,1);
		state_power_P_reactive.grow_to(n_states,1);
#define Z 0
#define I 1
#define P 2
		for ( size_t n=0; n<n_states ; n++ ) // each state
		{
	//		for ( size_t m=0 ; m<3 ; m++ ) // ZIP components
				if ( n!=OFF ) {// on states is controller load by default
					state_power_Z_real.set_at(n,0,&control_power_Z_real);
					state_power_I_real.set_at(n,0,&control_power_I_real);
					state_power_P_real.set_at(n,0,&control_power_P_real);
					state_power_Z_reactive.set_at(n,0,&control_power_Z_reactive);
					state_power_I_reactive.set_at(n,0,&control_power_I_reactive);
					state_power_P_reactive.set_at(n,0,&control_power_P_reactive);
				}
				else{
					state_power_Z_real.set_at(n,0,(double)0);
					state_power_I_real.set_at(n,0,(double)0);
					state_power_P_real.set_at(n,0,(double)0);
					state_power_Z_reactive.set_at(n,0,(double)0);
					state_power_I_reactive.set_at(n,0,(double)0);
					state_power_P_reactive.set_at(n,0,(double)0);
				}
	//		}
		}
		state_power_Z_real.set_at(PUMPPREWASH,0,&pump_power_Z_real);
		state_power_I_real.set_at(PUMPPREWASH,0,&pump_power_I_real);
		state_power_P_real.set_at(PUMPPREWASH,0,&pump_power_P_real);
		state_power_Z_reactive.set_at(PUMPPREWASH,0,&pump_power_Z_reactive);
		state_power_I_reactive.set_at(PUMPPREWASH,0,&pump_power_I_reactive);
		state_power_P_reactive.set_at(PUMPPREWASH,0,&pump_power_P_reactive);
		state_power_Z_real.set_at(HEATWASHQUICK,0,&coil_power_wet_real);
		state_power_Z_reactive.set_at(HEATWASHQUICK,0,&coil_power_wet_reactive);
		state_power_Z_real.set_at(HEATWASHNORMAL,0,&coil_power_wet_real);
		state_power_Z_reactive.set_at(HEATWASHNORMAL,0,&coil_power_wet_reactive);
		state_power_Z_real.set_at(HEATWASHHEAVY,0,&coil_power_wet_real);
		state_power_Z_reactive.set_at(HEATWASHHEAVY,0,&coil_power_wet_reactive);
		state_power_P_real.set_at(HEATWASHQUICK,0,&pump_power_P_real);
		state_power_P_reactive.set_at(HEATWASHQUICK,0,&pump_power_P_reactive);
		state_power_P_real.set_at(HEATWASHNORMAL,0,&pump_power_P_real);
		state_power_P_reactive.set_at(HEATWASHNORMAL,0,&pump_power_P_reactive);	
		state_power_P_real.set_at(HEATWASHHEAVY,0,&pump_power_P_real);
		state_power_P_reactive.set_at(HEATWASHHEAVY,0,&pump_power_P_reactive);
		state_power_P_real.set_at(PUMPWASHQUICK,0,&pump_power_P_real);
		state_power_P_reactive.set_at(PUMPWASHQUICK,0,&pump_power_P_reactive);
		state_power_P_real.set_at(PUMPWASHQUICK,0,&pump_power_P_real);
		state_power_P_reactive.set_at(PUMPWASHQUICK,0,&pump_power_P_reactive);
		state_power_P_real.set_at(PUMPWASHNORMAL,0,&pump_power_P_real);
		state_power_P_reactive.set_at(PUMPWASHNORMAL,0,&pump_power_P_reactive);
		state_power_P_real.set_at(PUMPWASHHEAVY,0,&pump_power_P_real);
		state_power_P_reactive.set_at(PUMPWASHHEAVY,0,&pump_power_P_reactive);
		state_power_P_real.set_at(PUMPRINSE,0,&pump_power_P_real);
		state_power_P_reactive.set_at(PUMPRINSE,0,&pump_power_P_reactive);
		state_power_Z_real.set_at(HEATRINSE,0,&coil_power_wet_real);
		state_power_Z_reactive.set_at(HEATRINSE,0,&coil_power_wet_reactive);
		state_power_P_real.set_at(HEATRINSE,0,&pump_power_P_real);
		state_power_P_reactive.set_at(HEATRINSE,0,&pump_power_P_reactive);
		state_power_Z_real.set_at(HEATDRYON,0,&coil_power_wet_real);
		state_power_Z_reactive.set_at(HEATDRYON,0,&coil_power_wet_reactive);
		state_power_Z_real.set_at(HEATDRYOFF,0,&coil_power_dry_real);
		state_power_Z_reactive.set_at(HEATDRYOFF,0,&coil_power_dry_reactive);

	//	state_power.set_at(HEATWASHQUICK,Z,&coil_power_wet);
	//	state_power.set_at(HEATWASHNORMAL,Z,&coil_power_wet);
	//	state_power.set_at(HEATWASHHEAVY,Z,&coil_power_wet);
	//	state_power.set_at(HEATWASHQUICK,P,&pump_power);
	//	state_power.set_at(HEATWASHNORMAL,P,&pump_power);
	//	state_power.set_at(HEATWASHHEAVY,P,&pump_power);
	//	state_power.set_at(PUMPWASHQUICK,P,&pump_power);
	//	state_power.set_at(PUMPWASHNORMAL,P,&pump_power);
	//	state_power.set_at(PUMPWASHHEAVY,P,&pump_power);
	//	state_power.set_at(HEATWASHQUICK,P,&pump_power);
	//	state_power.set_at(HEATWASHNORMAL,P,&pump_power);
	//	state_power.set_at(HEATWASHHEAVY,P,&pump_power);

	//	state_power.set_at(PUMPRINSE,P,&pump_power);
	//	state_power.set_at(HEATRINSE,Z,&coil_power_wet);
	//	state_power.set_at(HEATRINSE,P,&pump_power);
	//	state_power.set_at(HEATDRYON,Z,&coil_power_dry);
	//	state_power.set_at(HEATDRYOFF,Z,&coil_power_dry);
	}
	
	// check the size of the state_power array 
	if ( state_power_Z_real.get_rows()!=n_states )
	{	
		exception("state_power array does not have the same number of rows as the controlmode enumeration has possible values");
	}
	if ( state_power_Z_real.get_cols()!=1 )
		exception("state_power array does not have the 3 columns for Z, I and P components ");

	// check whether the machine programmed by user
	if ( state_machine.variable==NULL ) 
	{	
		// install default program

		gld_property fsm = get_state_machine_property();
		fsm.from_string("state:controlmode");
		fsm.from_string("interval:60");
		fsm.from_string("rule:OFF->CONTROLSTART=state_queue>0"); 
		fsm.from_string("rule:CONTROLSTART->PUMPPREWASH=$timer>state_duration#controlmode.CONTROLSTART");
		fsm.from_string("rule:PUMPPREWASH->HEATWASHQUICK=$timer>state_duration#controlmode.PUMPPREWASH");
		fsm.from_string("rule:HEATWASHQUICK->PUMPWASHQUICK=$timer>state_duration#controlmode.HEATWASHQUICK");
		fsm.from_string("rule:PUMPWASHQUICK->CONTROLWASH=$timer>state_duration#controlmode.PUMPWASHQUICK");

		fsm.from_string("rule:CONTROLWASH->HEATRINSE=$timer>state_duration#controlmode.CONTROLWASH, washmode==#washmode.QUICK");
		//fsm.from_string("rule:CONTROLWASH->HEATRINSE=$timer>state_duration#CONTROLWASH, washmode==0x01");
		fsm.from_string("rule:HEATRINSE->PUMPRINSE=$timer>state_duration#controlmode.HEATRINSE");
		fsm.from_string("rule:PUMPRINSE->CONTROLRINSE=$timer>state_duration#controlmode.PUMPRINSE");
		fsm.from_string("rule:CONTROLRINSE->OFF=$timer>state_duration#controlmode.CONTROLRINSE, drymode==#drymode.AIR");

		fsm.from_string("rule:CONTROLWASH->HEATWASHNORMAL=$timer>state_duration#controlmode.CONTROLWASH, washmode==#drymode.NORMAL");
		//fsm.from_string("rule:CONTROLWASH->HEATWASHNORMAL=$timer>state_duration#CONTROLWASH, washmode==0x02");
		fsm.from_string("rule:HEATWASHNORMAL->PUMPWASHNORMAL=$timer>state_duration#controlmode.HEATWASHNORMAL");
		fsm.from_string("rule:PUMPWASHNORMAL->CONTROLRINSE=$timer>state_duration#controlmode.PUMPWASHNORMAL");

		fsm.from_string("rule:CONTROLWASH->HEATWASHHEAVY=$timer>state_duration#controlmode.CONTROLWASH, washmode==#washmode.HEAVY");
		fsm.from_string("rule:HEATWASHHEAVY->PUMPWASHHEAVY=$timer>state_duration#controlmode.HEATWASHHEAVY");
		fsm.from_string("rule:PUMPWASHHEAVY->CONTROLRINSE=$timer>state_duration#contromode.PUMPWASHHEAVY");
		// TODO change this temperature driven duty-cycle, not time-driver

		fsm.from_string("rule:CONTROLRINSE->HEATDRYON=$timer>state_duration#controlmode.CONTROLRINSE, drymode==#drymode.HEAT");
		fsm.from_string("rule:HEATDRYON->CONTROLDRY=$timer>state_duration#controlmode.HEATDRYON");
		fsm.from_string("rule:CONTROLDRY->HEATDRYOFF=$timer>state_duration#controlmode.CONTROLDRY");
		fsm.from_string("rule:HEATDRYOFF->CONTROLEND=$timer>state_duration#controlmode.HEATDRYOFF");
		fsm.from_string("rule:CONTROLEND->OFF=$timer>state_duration#controlomode.CONTROLEND");
	}

	// setup transition default call to decrement queue
	struct {
		DISHWASHERSTATE state;
		FSMCALL call;
	} *f, map[] = {
		{OFF,				dishwasher_decrement_queue,		NULL,						NULL},
		{PUMPPREWASH,		dishwasher_fill_unit,			dishwasher_check_fill,		dishwasher_drain_unit}, // note: may use cold water
		{PUMPWASHQUICK,		dishwasher_fill_unit,			dishwasher_check_fill,		NULL},
		{HEATWASHQUICK,		dishwasher_heat_water,			dishwasher_check_water,		dishwasher_drain_unit},
		{PUMPWASHNORMAL,	dishwasher_fill_unit,			dishwasher_check_fill,		NULL},
		{HEATWASHNORMAL,	dishwasher_heat_water,			dishwasher_check_water,		dishwasher_drain_unit},
		{PUMPWASHHEAVY,		dishwasher_fill_unit,			dishwasher_check_fill,		NULL},
		{HEATWASHHEAVY,		dishwasher_heat_water,			dishwasher_check_water,		dishwasher_drain_unit},
		{CONTROLWASH,		NULL,							NULL,						NULL},
		{PUMPRINSE,			dishwasher_fill_unit,			dishwasher_check_fill,		NULL},
		{HEATRINSE,			dishwasher_heat_water,			dishwasher_check_water,		dishwasher_drain_unit},
		{CONTROLRINSE,		NULL,							NULL,						NULL},
		{HEATDRYON,			dishwasher_heat_air,			dishwasher_check_air,		NULL},
		{HEATDRYOFF,		NULL,							NULL,						NULL},
		{CONTROLDRY,		NULL,							NULL,						NULL},
		{CONTROLEND,		NULL,							NULL,						NULL},
	};
	for ( size_t n=0 ; n<sizeof(map)/sizeof(map[0]) ; n++ )
	{
		f = map+n;
		state_machine.transition_calls[f->state].entry = f->call.entry;
		state_machine.transition_calls[f->state].during = f->call.during;
		state_machine.transition_calls[f->state].exit = f->call.exit;
	}

	return residential_enduse::init(parent);
}

int dishwasher::isa(char *classname)
{
	return (strcmp(classname,"dishwasher")==0 || residential_enduse::isa(classname));
}

int dishwasher::precommit(TIMESTAMP t1)
{
	//TO DO: GPM FOR DISHWASHER
	state_queue += demand_rate * (double)(t1 - my()->clock) / 86400;
	return 1;
}

TIMESTAMP dishwasher::sync(TIMESTAMP t1) 
{	
	load.admittance.SetRect(state_power_Z_real.get_at((size_t)controlmode,0), state_power_Z_reactive.get_at((size_t)controlmode,0));
	load.current.SetRect(state_power_I_real.get_at((size_t)controlmode,0), state_power_I_reactive.get_at((size_t)controlmode,0));
	load.power.SetRect(state_power_P_real.get_at((size_t)controlmode,0), state_power_P_reactive.get_at((size_t)controlmode,0));
	//load.current = state_power_I_real.get_at((size_t)controlmode,0);
	//load.power = state_power_P_real.get_at((size_t)controlmode,0);
	load.demand = load.power + (load.current + load.admittance*load.voltage_factor)*load.voltage_factor;
	load.total = load.power + load.current + load.admittance;
	load.heatgain = load.total.Mag() * load.heatgain_fraction;
	// TODO add these into variable list and document
	//total_power = (load.power.Re() + (load.current.Re() + load.admittance.Re()*load.voltage_factor)*load.voltage_factor) * 1000;
//	total_power=(load.power+(load.current+load.admittance*load.voltage_factor)*load.voltage_factor)*1000;
	total_admittance = load.admittance;
	total_current = load.current;
	total_power = load.power;
	return residential_enduse::sync(my()->clock,t1);
}

TIMESTAMP dishwasher::presync(TIMESTAMP t1)
{
	//
	return TS_NEVER; // fsm takes care of reporting next event time
}

/**@}**/
