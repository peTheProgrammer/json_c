/*
	This is written in one file so that it can be ported easily to another project.
	To do so put the file in the other project and before including it define JSON_IMPLEMENTATION
	for the implementation to be compiled.
	Do not define JSON_IMPLEMENTATION in multiple files because the functions will be defined multiple times
	which will lead to linker errors.
	The best way to do it is to create a .c file in which the only thing you do is define JSON_IMPLEMENTATION and then include this file.
	Or copy paste the following two lines : 
#define JSON_IMPLEMENTATION
#include "json.h"
*/

// enable only in debug
// printing slows down the functions by about 10x
// 1 -> enabled
// 0 -> disabled
#define JSON_PRINTING 0

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#define JSON_TYPE_NONE 0
#define JSON_TYPE_INTEGER 1
#define JSON_TYPE_FLOAT 2
#define JSON_TYPE_STRING 3
#define JSON_TYPE_BOOLEAN 4
#define JSON_TYPE_ARRAY 5
#define JSON_TYPE_OBJECT 6

typedef int64_t JSON_Integer;
typedef double JSON_Float;
typedef char *JSON_String;
typedef uint64_t JSON_Boolean;
typedef void *JSON_Array;
typedef void *JSON_Object;

// external functions
void *json_parse(char *data);
char *json_stringify(void *obj);

const uint64_t *json_get_value(const uint64_t *var, const char *name);
//////////////////////////////////

//#ifdef JSON_IMPLEMENTATION

#if JSON_PRINTING
#define JSON_PRINT(...) \
	fprintf(stdout, __VA_ARGS__)
#define JSON_PRINT_ERROR(...) \
	fprintf(stderr, __VA_ARGS__)
#else
#define JSON_PRINT(...)
#define JSON_PRINT_ERROR(...)
#endif

// get value

uint16_t _json_name_length(const char *name)
{
	uint16_t len = 0;
	while (name[len] != '.' && name[len] != '\0')
		len++;
	return len;
}

uint16_t _json_name_compare(const char *name1, const char *name2)
{
	uint16_t len1 = _json_name_length(name1);
	uint16_t len2 = _json_name_length(name2);
	if (len1 != len2)
		return 0;
	else
	{
		for (uint16_t i = 0; i < len1; i++)
			if (name1[i] != name2[i])
				return 0;
	}
	return 1;
}

uint64_t _json_name_to_index(const char *name)
{
	uint64_t index = 0;
	uint64_t i = 0;
	while (name[i] != '.' && name[i] != '\0')
	{
		index *= 10;
		index += name[i] - '0';
		i++;
	}
	return index;
}

const uint64_t *json_get_value(const uint64_t *var, const char *name)
{

	uint64_t type = var[0];
	JSON_PRINT("type = %lu\n", type);
	switch (type)
	{
	case JSON_TYPE_BOOLEAN:
	case JSON_TYPE_FLOAT:
	case JSON_TYPE_INTEGER:
	case JSON_TYPE_STRING:
	{
		return var + 1;
	}
	case JSON_TYPE_OBJECT:
	{
		uint64_t *data = (uint64_t*)var[1];
		uint64_t count = data[-1];

		for (uint64_t i = 0; i < count; i++)
		{
			const char *_name = (const char *)data[i * 3];
			JSON_PRINT("name = %s\n", _name);
			uint64_t _type = data[i * 3 + 1];
			if (_json_name_compare(_name, name))
				return json_get_value(data + i * 3 + 1, name + _json_name_length(name) + 1);
		}
		JSON_PRINT_ERROR("JSON::Object Name '%s' not found.\n", name);
		break;
	}
	case JSON_TYPE_ARRAY:
	{
		uint64_t *data = (uint64_t*)var[1];
		uint64_t count = data[-1];
		uint64_t index = _json_name_to_index(name);

		if (index < 0 || index >= count)
			JSON_PRINT_ERROR("JSON::Array Out of bounds.\nindex : %ld, count : %ld\n", index, count);
		else
			return json_get_value(data + index * 2, name + _json_name_length(name) + 1);

		break;
	}
	default:
		JSON_PRINT_ERROR("JSON type not found.\ntype : %ld\n", type);
		break;
	}
	return NULL;
}

uint64_t json_get_size(const uint64_t *var, const char *name)
{
	uint64_t type = var[0];
	JSON_PRINT("type = %lu\n", type);
	switch (type)
	{
	case JSON_TYPE_BOOLEAN:
	case JSON_TYPE_FLOAT:
	case JSON_TYPE_INTEGER:
	case JSON_TYPE_STRING:
	{
		return 1;
	}
	case JSON_TYPE_OBJECT:
	{
		uint64_t *data = (uint64_t*)var[1];
		uint64_t count = data[-1];

		for (uint64_t i = 0; i < count; i++)
		{
			const char *_name = (const char *)data[i * 3];
			JSON_PRINT("name = %s\n", _name);
			uint64_t _type = data[i * 3 + 1];
			if (_json_name_compare(_name, name))
				return json_get_size(data + i * 3 + 1, name + _json_name_length(name) + 1);
		}
		JSON_PRINT_ERROR("JSON::Object Name '%s' not found.\n", name);
		break;
	}
	case JSON_TYPE_ARRAY:
	{
		uint64_t *data = (uint64_t*)var[1];
		uint64_t count = data[-1];
		uint64_t index = _json_name_to_index(name);

		if (index < 0 || index >= count)
			JSON_PRINT_ERROR("JSON::Array Out of bounds.\nindex : %ld, count : %ld\n", index, count);
		else
			return json_get_size(data + index * 2, name + _json_name_length(name) + 1);

		break;
	}
	default:
		JSON_PRINT_ERROR("JSON type not found.\ntype : %ld\n", type);
		break;
	}
	return 0;
}

// parsing

typedef struct
{
	uint16_t line_count;
	uint16_t terminate;
	char *cdata;
	char *end_of_data;
	char *strings;
	uint64_t *objects;
	uint32_t object_length;
	uint32_t array_length;
	uint32_t string_length;
	uint16_t *stack;
	uint16_t *counts;
	uint16_t tos;
	uint16_t count_index;
} JSON_Parsing_Data;

void _json_parse_variable(JSON_Parsing_Data *parse, uint16_t depth, uint64_t *var);

// subfunctions
#define _JSON_PARSE_PRINT_ERROR(...)                                                  \
	{                                                                                 \
		JSON_PRINT_ERROR(__VA_ARGS__);                                                \
		JSON_PRINT_ERROR(" on LINE %d and VARIABLE %s\n", parse->line_count, "None"); \
		parse->terminate = 1;                                                         \
	}

#define _JSON_PARSE_NEXT_CHAR()                   \
	if (*(parse->cdata) == '\n')                  \
		parse->line_count++;                      \
	else if (parse->cdata >= parse->end_of_data)  \
	{                                             \
		_JSON_PARSE_PRINT_ERROR("Out of bounds"); \
	}                                             \
	(parse->cdata)++

#define _JSON_PARSE_SKIP_WHITESPACE()                                                                       \
	while (*parse->cdata == ' ' || *parse->cdata == '\t' || *parse->cdata == '\n' || *parse->cdata == '\r') \
	{                                                                                                       \
		_JSON_PARSE_NEXT_CHAR();                                                                            \
	}

#define _JSON_PARSE_STRING()     \
	parse->cdata;                \
	while (*parse->cdata != '"') \
	{                            \
		(parse->cdata)++;        \
	}

#define _JSON_PARSE_DEPTH_PRINT(...)            \
	if (parse->line_count < 50)                 \
	{                                           \
		for (uint16_t _i = 0; _i < depth; _i++) \
			JSON_PRINT("\t");                   \
		JSON_PRINT(__VA_ARGS__);                \
	}

#define JSON_PARSE_READING_STRING '"'
#define JSON_PARSE_READING_OBJECT '{'
#define JSON_PARSE_READING_ARRAY '['

void _json_parse_calculate_sizes(JSON_Parsing_Data *parse, uint16_t depth)
{
	JSON_PRINT("count_index = %u\n", parse->count_index);
	uint16_t *count = parse->counts + parse->count_index;
	parse->count_index++;
	*count = 0;
	uint16_t bracket;
	parse->stack[++(parse->tos)] = *parse->cdata;
	do
	{
		parse->cdata++;
		bracket = *parse->cdata == '}' || *parse->cdata == ']';
		if (*parse->cdata == '"')
		{
			uint16_t length = 0;
			do
			{
				length++;
			} while (parse->cdata[length] != '"');
			parse->string_length += length;
			parse->cdata += length;
		}
		else if (*parse->cdata == '{')
		{
			_json_parse_calculate_sizes(parse, depth + 1);
		}
		else if (*parse->cdata == '[')
		{
			_json_parse_calculate_sizes(parse, depth + 1);
		}
		else if (*parse->cdata == ',' | bracket)
		{
			(*count)++;
			if (parse->stack[parse->tos] == JSON_PARSE_READING_ARRAY)
			{
				(parse->array_length) += 2;
				if (bracket)
					(parse->array_length)++;
			}
			else if (parse->stack[parse->tos] == JSON_PARSE_READING_OBJECT)
			{
				(parse->object_length) += 3;
				if (bracket)
					(parse->object_length)++;
			}
			_JSON_PARSE_DEPTH_PRINT("count++, %u, index %lu\n", *count, (uint64_t)(count - parse->counts));
		}
	} while (!bracket && (*parse->cdata) != '\0');
	(parse->tos)--;
}

void *json_parse(char *data)
{

	JSON_Parsing_Data parse =
		{
			.line_count = 0,
			.terminate = 0,
			.cdata = data,
			.end_of_data = data + strlen(data),
			.strings = NULL,
			.objects = NULL,
			.object_length = 0,
			.array_length = 0,
			.string_length = 0,
			.stack = malloc(200),
			.counts = malloc(200),
			.tos = -1,
			.count_index = 0};

	// find the sizes of memory to allocate
	_json_parse_calculate_sizes(&parse, 0);

	// allocate the memory
	parse.strings = malloc(parse.string_length);
	parse.objects = malloc((parse.array_length + parse.object_length + 2) * sizeof(uint64_t));

	JSON_PRINT("object_length = %u\n", parse.object_length);
	JSON_PRINT("array_length = %u\n", parse.array_length);
	JSON_PRINT("string_length = %u\n", parse.string_length);
	for (uint16_t i = 0; i < parse.count_index; i++)
		JSON_PRINT("counts%u = %u\n", i, parse.counts[i]);

	// reset values
	parse.line_count = 0;
	parse.cdata = data;
	parse.tos = -1;
	parse.count_index = 0;

	// make room for root variable
	uint64_t *objects = parse.objects;
	parse.objects += 2;

	// parse
	_json_parse_variable(&parse, 0, parse.objects - 2);

	// free memory
	free(parse.stack);
	free(parse.counts);

	// return
	return objects;
}

uint32_t _json_parse_strcmp(char *s1, char *s2)
{
	uint32_t len = strlen(s2);
	for (uint32_t i = 0; i < len; i++)
		if (s1[i] != s2[i])
			return 0;
	return 1;
}

void _json_parse_variable(JSON_Parsing_Data *parse, uint16_t depth, uint64_t *var)
{

	// find var type
	switch (*parse->cdata)
	{
	case '{':
	{
		// set type
		var[0] = JSON_TYPE_OBJECT;
		// get count
		uint64_t count = parse->counts[(parse->count_index)++];
		// set pointer to vars
		uint64_t *vars = parse->objects + 1;
		var[1] = (uint64_t) vars;
		// 'allocate' memory for object
		parse->objects = vars + count * 3;
		// set count
		vars[-1] = count;

		// start counter
		for(uint64_t i = 0; i < count; i++)
		//while (*parse->cdata != '}' && parse->cdata <= parse->end_of_data)
		{
			_JSON_PARSE_NEXT_CHAR();
			_JSON_PARSE_SKIP_WHITESPACE();
			if (*parse->cdata == '}')
				break;
			_JSON_PARSE_NEXT_CHAR();
			char *name = _JSON_PARSE_STRING();
			uint64_t name_length = (uint64_t)(parse->cdata - name);
			_JSON_PARSE_DEPTH_PRINT("name_length : %lu\n", name_length);
			memcpy(parse->strings, name, name_length);
			vars[i * 3] = (uint64_t) parse->strings;
			parse->strings += name_length + 1;
			parse->strings[-1] = '\0';
			_JSON_PARSE_NEXT_CHAR();

			_JSON_PARSE_SKIP_WHITESPACE();
			if (*parse->cdata == ':')
			{
				_JSON_PARSE_NEXT_CHAR();
				_JSON_PARSE_SKIP_WHITESPACE();
				_json_parse_variable(parse, depth + 1, vars + i * 3 + 1);
				_JSON_PARSE_DEPTH_PRINT("name : %s, type : %lu, value : %lu\n", (char *)vars[i * 3], vars[i * 3 + 1], vars[i * 3 + 2]);
			}
			else
				_JSON_PARSE_PRINT_ERROR("Expected :");
			_JSON_PARSE_SKIP_WHITESPACE();
			if (*parse->cdata == ',')
				continue;
			else if (*parse->cdata == '}')
				break;
			else
				_JSON_PARSE_PRINT_ERROR("Expected , or } after variable but got %c, %u ", *parse->cdata, *parse->cdata);
		}
		_JSON_PARSE_NEXT_CHAR();
		break;
	}
	case '[':
	{
		// set type
		var[0] = JSON_TYPE_ARRAY;
		// get count
		uint64_t count = parse->counts[(parse->count_index)++];
		// set pointer to vars
		uint64_t *vars = parse->objects + 1;
		var[1] = (uint64_t) vars;
		// 'allocate' memory for array
		parse->objects = vars + count * 2;
		// set count
		vars[-1] = count;

		for(uint64_t i = 0; i < count; i++)
		//while (*parse->cdata != ']' && parse->cdata <= parse->end_of_data)
		{
			_JSON_PARSE_NEXT_CHAR(); // skip [ or ,
			_JSON_PARSE_SKIP_WHITESPACE();
			_json_parse_variable(parse, depth + 1, vars + i * 2);
			_JSON_PARSE_SKIP_WHITESPACE();
			if (*parse->cdata == ',')
				continue;
			else if (*parse->cdata == ']')
				break;
			else
			{
				_JSON_PARSE_PRINT_ERROR("Expected , or ] after variable but got %c, %u ", *parse->cdata, *parse->cdata);
			}
		}
		_JSON_PARSE_NEXT_CHAR();
		break;
	}
	case 't':
	case 'f':
	{
		var[0] = JSON_TYPE_BOOLEAN;

		char *start = parse->cdata;
		if (_json_parse_strcmp(start, "true"))
		{
			var[1] = 1;
			for (uint16_t i = 0; i < 4; i++)
			{
				_JSON_PARSE_NEXT_CHAR();
			}
		}
		else if (_json_parse_strcmp(start, "false"))
		{
			var[1] = 0;
			for (uint16_t i = 0; i < 5; i++)
			{
				_JSON_PARSE_NEXT_CHAR();
			}
		}
		else
			_JSON_PARSE_PRINT_ERROR("Expected true or false");
		break;
	}
	case '-':
	case '0':
	case '1':
	case '2':
	case '3':
	case '4':
	case '5':
	case '6':
	case '7':
	case '8':
	case '9':
	{
		uint64_t *type = var;
		uint64_t *value = var + 1;

		uint16_t sign = 1;
		JSON_Integer number = 0;
		if(*parse->cdata == '-'){
			sign = -1;
			parse->cdata++;
		}
		while(*parse->cdata >= '0' && *parse->cdata <= '9')
		{
			number *= 10;
			number += *parse->cdata - '0';
			parse->cdata++;
		}
		if(*parse->cdata == '.')
		{
			parse->cdata++;
			double mantis = 0.0;
			double dividor = 1.0;
			while(*parse->cdata >= '0' && *parse->cdata <= '9')
			{
				dividor *= 10;
				mantis += (double)(*parse->cdata - '0') / dividor;
				parse->cdata++;
			}

			*type = JSON_TYPE_FLOAT;
			JSON_Float *float_pointer = (JSON_Float*) value;
			JSON_PRINT("number = %lu, mantis = %lf\n", number, mantis);
			*float_pointer = (double)number + mantis;
		}
		else
		{
			*type = JSON_TYPE_INTEGER;
			JSON_Integer *int_pointer = (JSON_Integer*) value;
			*int_pointer = number * sign;
		}
		break;
	}
	case '"':
	{
		var[0] = JSON_TYPE_STRING;
		char **value = (char **)(var + 1);

		_JSON_PARSE_NEXT_CHAR();
		char *string = _JSON_PARSE_STRING();
		uint64_t name_length = (uint64_t)(parse->cdata - string);
		memcpy(parse->strings, string, name_length);
		*value = parse->strings;
		parse->strings += name_length;
		*parse->strings = '\0';
		parse->strings++;
		_JSON_PARSE_NEXT_CHAR();

		break;
	}
	default:
		break;
	}

// undef subfunctions
#undef _JSON_PARSE_PRINT_ERROR
#undef _JSON_PARSE_NEXT_CHAR
#undef _JSON_PARSE_SKIP_WHITESPACE
#undef _JSON_PARSE_STRING
#undef _JSON_PARSE_DEPTH_PRINT
}

//#endif