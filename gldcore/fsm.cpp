// $Id: fsm.cpp 4984 2014-12-19 22:05:13Z dchassin $
// Finite State Machine
//

#include <ctype.h>
#include "module.h"
#include "output.h"
#include "fsm.h"

#define MAXSTATES 256
static statemachine *first_machine = NULL;
static unsigned int n_machines = 0;

double cast_double(PROPERTYTYPE ptype, void *addr, size_t index=0)
{
	switch (ptype) 
	{
		case PT_void: return 0.0;
		case PT_double: return (double)((double*)addr)[index];
		case PT_complex: return (double)((char*)addr)[index];
		case PT_enumeration: return (double)((enumeration*)addr)[index];
		case PT_set: return (double)((set*)addr)[index];
		case PT_int16: return (double)((int16*)addr)[index];
		case PT_int32: return (double)((int32*)addr)[index];
		case PT_int64: return (double)((int64*)addr)[index];
		case PT_char8: return (double)((char*)addr)[index];
		case PT_char32: return (double)((char*)addr)[index];
		case PT_char256: return (double)((char*)addr)[index];
		case PT_char1024: return (double)((char*)addr)[index];
		//case PT_object: return (double)((object*)addr)[index];
		//case PT_delegated: return (double)((delegated*)addr)[index];
		case PT_bool: return (double)((bool*)addr)[index];
		case PT_timestamp: return (double)((char*)addr)[index];
		case PT_double_array: return (double)((double*)addr)[index];
	//	case PT_complex_array: return (double)((complex*)addr)[index];
		case PT_real: return (double)((real*)addr)[index];
		case PT_float: return (double)((float*)addr)[index];
	//	case PT_loadshape: return (double)((loadshape*)addr)[index];
	//	case PT_enduse: return (double)((enduse*)addr)[index];
	//	case PT_random: return (double)((random*)addr)[index];
	//	case PT_statemachine: return (double)((statemachine*)addr)[index]; 
		default:
			throw "cast_double given unsupported property type";
	}
}

extern "C" int fsm_create(statemachine *machine, OBJECT *context)
{
	// set up
	memset(machine,0,sizeof(statemachine));
	machine->next = first_machine;
	first_machine = machine;
	n_machines++;

	// connect to object
	machine->context = context;
	machine->interval = 60;
	// done
	return 1;
}

extern "C" int fsm_init(statemachine *machine)
{
	machine->hold_time = TS_NEVER;
	machine->entry_time = global_clock;
	return 0;
}

extern "C" int fsm_initall(void)
{
	statemachine *machine;
	for ( machine=first_machine ; machine!=NULL ; machine=machine->next )
	{
		if ( fsm_init(machine)==1 )
			return FAILED;
	}
	return SUCCESS;
}

void fsm_do_entry(statemachine *machine)
{
	uint64 state = (uint64)(machine->value);
	if ( machine->transition_calls && machine->transition_calls[state].entry )
		machine->transition_calls[state].entry(machine);
}

void fsm_do_during(statemachine *machine)
{
	uint64 state = (uint64)(machine->value);
	if ( machine->transition_calls && machine->transition_calls[state].during )
		machine->transition_calls[state].during(machine);
}

void fsm_do_exit(statemachine *machine)
{
	uint64 state = (uint64)(machine->value);
	if ( machine->transition_calls && machine->transition_calls[state].exit )
		machine->transition_calls[state].exit(machine);
}

extern "C" TIMESTAMP fsm_sync(statemachine *machine, PASSCONFIG pass, TIMESTAMP t1)
{
	if ( !fsm_isprogrammed(machine) )
		return TS_NEVER;

	int64 state = *machine->variable;

	// update timer
	machine->runtime = (double)(global_clock - machine->entry_time);

	// check hold time
	if ( global_clock<machine->hold_time && machine->hold_time<t1 )
		return machine->hold_time;

	// exit timer expired
	machine->hold_time = TS_NEVER;

	// process rules
	FSMRULE *rule = machine->transition_rules[state];
	while ( rule!=NULL ) // ; rule=(rule->next_and==NULL?rule->next_or:rule->next_and) )
	{
		// get test data
		double a = cast_double(rule->lhtype,rule->lhs);
		double b = cast_double(rule->rhtype,rule->rhs);
		double tolerance = 0.0;
		output_debug("In SYNC, transition rule '%s' active", rule->spec );
		char tmp[1024];
		// if test fails
		char *copstr[] = {"==","<=",">=","!=","<",">","in","not in"};
		output_debug("Testing: %g %s %g ?", a, copstr[rule->cop], b);
		if ( property_compare_basic(PT_double,rule->cop,&a,&b,&tolerance,NULL) )
		{
			output_debug("Test passes");
			// last test in 'and' chain implies this is the rule to apply
			if ( rule->next_and==NULL )
			{	
				output_debug("Last test in 'and' chain implies this is the rule to apply. ");
				fsm_do_exit(machine);
				TIMESTAMP hold = (TIMESTAMP)machine->transition_holds->get_at((size_t)rule->to,0);
				machine->entry_time = t1;
				machine->hold_time = ( hold>0 && hold<TS_NEVER ? t1+hold : TS_NEVER );
				*(machine->variable) = (enumeration)(rule->to);
				rule = rule->next_or;
			}
			else
			{
				rule = rule->next_and;
			}
		}
		else
		{
			output_debug("Test fails");
			rule = rule->next_or;
		}
	}
	output_debug("No further tests.");

	// processing during call if no state change occurred
	if ( machine->value==*(machine->variable) )
	{
		output_debug("Remaining in this state");
		fsm_do_during(machine);
	}
	else
	{
		// copy state to local variable
		machine->value = *(machine->variable);

		// process entry call
		output_debug("Entering the new state");
		fsm_do_entry(machine);
	}

	// hold time is next transition time (soft event)
	TIMESTAMP t2 = ((t1/machine->interval)+1)*machine->interval;
	t2 = (machine->hold_time < TS_NEVER && machine->hold_time > t2 ? machine->hold_time : t2);
	char buffer[64];
	output_debug("FSM next time return is SOFT %s", convert_from_timestamp(t2,buffer,sizeof(buffer))?buffer:"(invalid)");
	return soften_timestamp(t2);
}

extern "C" TIMESTAMP fsm_syncall(TIMESTAMP t1)
{
	// TODO use MTI
	statemachine *machine;
	TIMESTAMP t2 = TS_NEVER;
	for (machine=first_machine; machine!=NULL; machine=machine->next)
	{
		TIMESTAMP t3 = fsm_sync(machine, PC_PRETOPDOWN, t1);
		if ( absolute_timestamp(t3)<absolute_timestamp(t2) ) t2 = t3;
	}
	return t2;
}

extern "C" int fsm_set_state_variable(statemachine *machine, char *name)
{
	char tmp[1024];

	// find the property
	machine->prop = object_get_property(machine->context,name,NULL);
	if ( machine->prop==NULL )
	{
		output_error("the finite state machine variable '%s' is not defined in the object '%s'", name, object_name(machine->context,tmp,sizeof(tmp)));
		return 0;
	}
	if ( machine->prop->ptype!=PT_enumeration )
	{
		output_error("finite state machine requires state variable '%s.%s' be an enumeration", object_name(machine->context,tmp,sizeof(tmp)),name);
		return 0;
	}
	machine->variable = object_get_enum(machine->context,machine->prop);

	// count the number of states
	KEYWORD *key;
	for ( key=machine->prop->keywords ; key!=NULL ; key=key->next )
	{
		if ( key->value+1 > machine->n_states )
			machine->n_states = key->value+1;
		else if ( key->value<0 )
			output_warning("finite state machine will ignore states corresponding to negative values of state variable '%s.%s'", object_name(machine->context,tmp,sizeof(tmp)),name);
		else if ( key->value>MAXSTATES )
			output_warning("finite state machine will ignore states corresponding to values of state variable '%s.%s' larger than %d", object_name(machine->context,tmp,sizeof(tmp)),name, MAXSTATES);
	}
	if ( machine->n_states<2 )
	{
		output_error("finite state machine requires state variable '%s.%s' must have two or more distinct states", object_name(machine->context,tmp,sizeof(tmp)),name);
		return 0;
	}

	// allocate machine rules, calls array and hold data
	machine->transition_rules = (FSMRULE**)malloc(sizeof(FSMRULE*)*machine->n_states);
	memset(machine->transition_rules,0,sizeof(FSMRULE*)*machine->n_states);
	machine->transition_calls = (FSMCALL*)malloc(sizeof(FSMCALL)*machine->n_states);
	memset(machine->transition_calls,0,sizeof(FSMCALL)*machine->n_states);
	machine->transition_holds->grow_to(machine->n_states-1,0);
	return 1;
}

extern "C" int fsm_add_transition_rule(statemachine *machine, char *spec)
{
	// parse transition specs
	char tmp[1024];
	char from[64], to[64], test[1024];
	char *speccopy = new char[strlen(spec)+1];
	strcpy(speccopy,spec);

	if ( sscanf(spec,"%[A-Za-z0-9_]->%[A-Za-z0-9_]=%[^\n]",from,to,test)<3 )
	{
		output_error("finite state machine in '%s' rule '%s' is invalid", object_name(machine->context,tmp,sizeof(tmp)),spec);
		return 0;
	}

	// get from and to keys
	uint64 from_key, to_key;
	if ( !property_get_keyword_value(machine->prop,from,&from_key) || from_key>MAXSTATES )
	{
		output_error("finite state machine in '%s' rule '%s' uses invalid from state '%s'", object_name(machine->context,tmp,sizeof(tmp)),spec,from);
		return 0;
	}
	if ( !property_get_keyword_value(machine->prop,to,&to_key) || to_key>MAXSTATES )
	{
		output_error("finite state machine in '%s' rule '%s' uses invalid from state '%s'", object_name(machine->context,tmp,sizeof(tmp)),spec,to);
		return 0;
	}

	FSMRULE *rule = NULL;

	// previous or
	FSMRULE *next_or = machine->transition_rules[from_key];

	// parse test
	char *token=NULL, *next=NULL;
	while ( (token=strtok_s(token==NULL?test:NULL,",",&next))!=NULL )
	{
		double *lvalue, *rvalue;
		PROPERTYCOMPAREOP cop;
		PROPERTYTYPE lptype = PT_void;
		PROPERTY *lhsprop = NULL;
		//PROPERTYTYPE lptype_double = PT_void;
		PROPERTYTYPE rptype = PT_void;
		output_debug("Parser: rule being parsed %s", token);
		// remote leading whitespace
		while ( *token!='\0' && isspace(*token) ) token++;
		if ( *token=='\0' )
		{
			output_warning("empty rule in '%s' rule '%s'", object_name(machine->context,tmp,sizeof(tmp)),spec);
			continue;
		}

		// extract operands and operator
		char lhs[64], rhs[64], op[8];
		if ( sscanf(token,"%[$-+A-Za-z0-9._#]%[<=>!]%[-+A-Za-z0-9._#]",lhs,op,rhs)!=3 )
		{
			output_error("finite state machine in '%s' test '%s' is invalid", object_name(machine->context,tmp,sizeof(tmp)),token);
			return 0;
		}

		// get left operand
		if ( strcmp(lhs,"$timer")==0 )
		{
			output_debug("FSM: In parser of the left hand side of expression (TIMER)");
			lvalue = &(machine->runtime);
			lptype = PT_double;
			lhsprop = object_get_property(machine->context,lhs, NULL);
			output_debug("finite state machine in '%s' test '%s' refers to property type '%d'", object_name(machine->context,tmp,sizeof(tmp)),token, lhsprop->name);
		}
		else if ( strcmp(lhs,"$state")==0 )
		{
			output_debug("FSM: In parser of the left hand side of expression (STATE)");
			lvalue = &(machine->value);
			lptype = PT_double;
		}
		else if ( !isalpha(lhs[0]) )
		{
			lvalue = new double;
			*lvalue = atof(lhs);
			lptype = PT_double;
			output_debug("FSM: In parser of the left hand side of expression (DOUBLE)");
		}
		else
		{	
			lhsprop = object_get_property(machine->context,lhs, NULL);
			output_debug("FSM: In parser of the left hand side of expression (KEYWORD PARSER)");
			unsigned int64 index = 0;
			char *pindex = strchr(lhs,'#');
			output_debug("FSM: In parser of the left hand side of expression, lhs : %s", lhs);
			output_debug("finite state machine in '%s' test '%s' refers to property type '%s'", object_name(machine->context,tmp,sizeof(tmp)),token, lhsprop->name);

			if ( pindex!=NULL )
			{
				// lookup keyword from state variable
				if ( !property_get_keyword_value(lhsprop,pindex+1,&index) )
				{
					output_error("finite state machine in '%s' test '%s' refers to non-existent state '%s' of property type '%s'", object_name(machine->context,tmp,sizeof(tmp)),token, pindex+1, machine->prop->name);
					return 0;
				}
				else // split string
					*pindex = '\0';
			}
			lvalue = object_get_double_by_name(machine->context,lhs);
			//output_debug("FSM: In parser of the left hand side of expression, lvalue : %s.%s = %f", object_name(machine->context,tmp,sizeof(tmp)),lhs,*lvalue);
			if ( lvalue==NULL )
			{
				output_error("finite state machine in '%s' test '%s' refers to non-existent property '%s'", object_name(machine->context,tmp,sizeof(tmp)),token, lhs);
				return 0;
			}
			else
				lvalue += index;

			
			if ( lhsprop==NULL )
			{
				output_error("finite state machine in '%s' test '%s' refers to invalid l-value property type in '%s'", object_name(machine->context,tmp,sizeof(tmp)),token, lhs);
				return 0;
			}
			lptype = lhsprop->ptype;
			output_debug("FSM: lptype: '%s'", property_getspec(lptype)->name);
		}

		// get right operand
		if ( strcmp(rhs,"$timer")==0 )
		{
			output_debug("FSM: In parser of the right hand side of expression (TIMER)");
			rvalue = &(machine->runtime);
			rptype = PT_double;
		}
		else if ( strcmp(rhs,"$state")==0 )
		{
			rvalue = &(machine->value);
			rptype = PT_double;
		}
		else if ( strchr("+-0123456789",rhs[0]) != NULL ) // constant
		{
			output_debug("FSM: In parser of the right hand side of expression (DOUBLE)");
			output_debug("FSM: '%s' rhs : '%s'", object_name(machine->context,tmp,sizeof(tmp)),token, rhs[0]);
			rvalue = new double;
			*rvalue = atof(rhs);
			rptype = PT_double;
			output_debug("FSM: In parser of the right hand side of expression (DOUBLE), rvalue: %s.%s = %f", object_name(machine->context,tmp,sizeof(tmp)), rhs, *rvalue);
		}
		else  // keyword
		{
			PROPERTY *rhsprop = machine->prop; // assume context of lhs, or target if lhs has none
			output_debug("In parser of the right hand side of expression (KEYWORD)");
			unsigned int64 index = 0;
			char *pindex = strchr(rhs,'#'); //search for hashtag (ie context)
			char *cindex = strchr(rhs,'.'); //search for period (ie keyword)
			//if ( pindex == rhs )
			//{
			//	rhsprop = lhsprop;
			//}
			output_debug("finite state machine in '%s' test '%s' refers to property type '%d'", object_name(machine->context,tmp,sizeof(tmp)), token, rhsprop->ptype);
			output_debug("RHS KEYWORD : %s and pindex is: %s", rhs, pindex);
			if ( pindex == NULL ) // missing #
			{
				output_error("finite state machine in '%s' test '%s' refers to r-value property missing keyword # mark in '%s'", object_name(machine->context,tmp,sizeof(tmp)),token, rhs);
				return 0;
			}
			if (cindex == NULL)
			{
				output_error("finite state machine in '%s' test '%s' refers to r-value property missing keyword . mark in '%s'", object_name(machine->context,tmp,sizeof(tmp)),token, rhs);
				return 0;
			}
			*pindex++ = '\0';
			*cindex++ = '\0';

			if ( rhs[0] != '#' ) // refers to an array (ex. state_duration#....)
			{
				if ( rhsprop == NULL )
					rhsprop = object_get_property(machine->context,rhs, NULL);
				output_debug("finite state machine in '%s' test '%s' refers to rhs property type '%d'", object_name(machine->context,tmp,sizeof(tmp)),token, rhsprop->name);
				if ( rhsprop==NULL )
				{
					output_error("finite state machine in '%s' test '%s' refers to invalid r-value property type in '%s'", object_name(machine->context,tmp,sizeof(tmp)),token, rhs);
					return 0;
				}
			}
			output_debug("FSM: RHS pindex is : %s ", pindex);
			
			// lookup keyword from state variable
			if ( !property_get_keyword_value(rhsprop,pindex,&index) )
			{
				output_error("finite state machine in '%s' test '%s' refers to non-existent state '%s' of '%s'", object_name(machine->context,tmp,sizeof(tmp)),token, pindex, rhsprop->name);
				return 0;
			}

			
			double_array *avalue = (double_array*)object_get_addr(machine->context,rhs[0]=='\0' ? lhs : rhs);

			if ( avalue==NULL )
			{
				output_error("finite state machine in '%s' test '%s' refers to non-existent property '%s'", object_name(machine->context,tmp,sizeof(tmp)),token, rhs);
				return 0;
			}
			else
			{
				if ( avalue == NULL )
				{
					output_error("finite state machine in '%s' test '%s' refers to invalid index %d in '%s'", object_name(machine->context,tmp,sizeof(tmp)),token, index, rhs);
					return 0;
				}
				else
				{
					rvalue = avalue->get_addr(index,0);

					if ( rvalue == NULL )
					{
						output_error("finite state machine in '%s' test '%s' refers to invalid r-value in '%s'", object_name(machine->context,tmp,sizeof(tmp)),token, rhs);
						return 0;
					}
				}
			}
			rptype = rhsprop->ptype;

		}

		// get operator for property type
		cop = property_compare_op(PT_double,op);

		// create rule
		rule = new FSMRULE;
		rule->to = to_key;
		rule->lhs = lvalue;
		rule->lhtype = lptype;
		rule->rhs = rvalue;
		rule->rhtype = rptype;
		rule->cop = cop;
		rule->spec = speccopy;
		output_debug("FSM: rule operator : %d ", cop);
		// link rule to last rule in state machine (or)
		if ( next_or!=NULL )
		{
			output_debug("FSM : OR rule is linked");
			rule->next_or = next_or;
			rule->next_and = NULL;
		}

		// link rule to previous rule in this transition (and)
		else
		{	
			output_debug("FSM: AND rule linked");
			rule->next_and = machine->transition_rules[from_key];
			rule->next_or = NULL;
		}
		machine->transition_rules[from_key] = rule;
	}

	return 1;
}

extern "C" int fsm_add_transition_hold(statemachine *machine, char *hold)
{
	char tmp[1024];
	char state[64], unit[256]="s";
	double duration;
	uint64 key;
	output_debug("Duration : '%s'", sscanf(hold,"%[A-Za-z0-9_]=%f%s",state,&duration,unit));


	if ( sscanf(hold,"%[A-Za-z0-9_]=%f%s",state,&duration,unit)<2 )
	{
		output_error("finite state machine in '%s' hold '%s' is not valid", object_name(machine->context,tmp,sizeof(tmp)),hold);
		return 0;
	}
	if ( !property_get_keyword_value(machine->prop,state,&key) || key>MAXSTATES )
	{
		output_error("finite state machine in '%s' hold '%s' uses a state value that is ignored", object_name(machine->context,tmp,sizeof(tmp)),hold);
		return 0;
	}
	
	// convert units
	if ( !unit_convert(unit,"s",&duration) )
	{
		output_error("finite state machine in '%s' hold '%s' uses duration unit '%s', which cannot be converted to seconds", object_name(machine->context,tmp,sizeof(tmp)),hold,unit);
		return 0;
	}

	// done
	machine->transition_holds->set_at((size_t)key,0,duration);
	return 1;
}

extern "C" int fsm_set_external_machine(statemachine *machine, char *spec)
{
	char tmp[1024];
	// TODO implement linkage
	output_error("%s: external finite state machine not implemented yet", object_name(machine->context,tmp,sizeof(tmp)));

//	char dllname[1024];
//	sprintf(dllname,"%s_%s.dll", machine->context->oclass->name, machine->context->name);

	return 0;
}

bool fsm_isprogrammed(statemachine *machine)
{ 
	return machine->variable!=NULL; 
};
void fsm_reset(statemachine *machine)
{
	if ( fsm_isprogrammed(machine) )
		machine->value = *(machine->variable) = 0;
	else
	{
		char tmp[64];
		output_warning("fsm_reset(machine.context='%s'): state machine is not programmed",
			object_name(machine->context,tmp,sizeof(tmp)));
	}
}

void fsm_clear(statemachine *machine)
{
	// release memory used by the machine
	if ( machine->transition_calls!=NULL ) free(machine->transition_calls);
	machine->transition_calls = NULL;
	for ( size_t n=0 ; n<machine->n_states ; n++ )
	{
		machine->transition_holds->set_at(n,0,(double*)NULL);
		struct s_fsmrule *rule=machine->transition_rules[n];
		while ( rule!=NULL )
		{
			struct s_fsmrule *next = ( rule->next_and ? rule->next_and : rule->next_or );
			free(rule);
			rule = next;
		}
		machine->transition_rules[n] = NULL;
	}

	// save important values
	OBJECT *context = machine->context;
	statemachine *next = machine->next;

	// obliterate the machine
	memset(machine,0,sizeof(statemachine));

	// restore important values
	machine->context = context;
	machine->next = next;
	machine->interval = 60;
}

void fsm_interval(statemachine *machine, char *interval)
{
	unsigned int64 value = atoi(interval);
	if ( value == 0 )
	{
		output_error("interval %s for statemachine is not valid, using NEVER",interval);
		machine->interval = TS_NEVER;
	}
	else
	{
		machine->interval = atoi(interval);
		output_debug("interval being processed: %d", machine->interval);
	}
}

extern "C" int convert_to_fsm(char *string, void *data, PROPERTY *prop)
{
	statemachine *machine= (statemachine*)data;

	// basic incoming double value (
	if ( isdigit(string[0]) )
	{
		machine->value = atof(string);
		return 1;
	}

	// copy string to local buffer because it's going to get trashed
	char buffer[1025];
	if (strlen(string)>sizeof(buffer)-1)
	{
		output_error("convert_to_fsm(string='%-.64s...',...) input string is too long", string);
		return 0;
	}
	strcpy(buffer,string);


	// parse tuples separated by semicolons
	char *token = NULL, *next = NULL;
	while ( (token=strtok_s(token==NULL?buffer:NULL,";",&next))!=NULL )
	{
		/* colon separate tuple parts */
		char *param = token;
		while ( *param!='\0' && (isspace(*param) || iscntrl(*param)) ) param++;
		if ( *param=='\0' ) break;

		/* isolate param and token and eliminte leading whitespaces */
		char *value = strchr(token,':');
		if (value==NULL)
			value="1";
		else
			*value++ = '\0'; /* separate value from param */
		while ( isspace(*value) || iscntrl(*value) ) value++;

		// parse params
		if ( strcmp(param,"value")==0 )
			machine->value = atof(value);
		else if ( strcmp(param,"state")==0 )
		{
			if ( !fsm_set_state_variable(machine,value) )
			{
				output_error("convert_to_fsm(string='%-.64s...',...) unable to set state variable name", string);
				return 0;
			}
		}
		else if ( strcmp(param,"reset")==0 )
		{
			fsm_reset(machine);
		}
		else if ( strcmp(param,"clear")==0 )
		{
			fsm_clear(machine);
		}
		else if ( strcmp(param,"interval")==0 )
		{
			fsm_interval(machine,value);
		}
		else if ( strcmp(param,"rule")==0 )
		{
			if ( !fsm_add_transition_rule(machine,value) )
			{
				output_error("convert_to_fsm(string='%-.64s...',...) unable to add transition rule", string);
				return -1;
			}
		}
		else if ( strcmp(param,"hold")==0 )
		{
			if ( !fsm_add_transition_hold(machine,value) )
			{
				output_error("convert_to_fsm(string='%-.64s...',...) unable to set transition hold", string);
				return 0;
			}
		}
		else if ( strcmp(param,"external")==0 )
		{
			if ( !fsm_set_external_machine(machine,value) )
			{
				output_error("convert_to_fsm(string='%-.64s...',...) unable to set external machine", string);
				return 0;
			}
		}
		else
		{
			output_error("convert_to_fsm(string='%-.64s...',...) '%s' is not recognized", string, param);
			return 0;
		}
	}
	return 1;
}
extern "C" int convert_from_fsm(char *buffer, int size, void *data, PROPERTY *prop)
{
	statemachine *machine = (statemachine*)data;
	return convert_from_double(buffer,size,&machine->value,prop);
}

