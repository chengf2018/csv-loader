#include <lua.h>
#include <lauxlib.h>
#include "csv.h"
#include <string.h>

static int pushvalue(lua_State *L, csv_value *value, int ignoreJson) {
	switch(value->type) {
		case TYPE_INT:
			lua_pushinteger(L, value->intvalue);
		break;
		case TYPE_DOUBLE:
			lua_pushnumber(L, value->doublevalue);
		break;
		case TYPE_STRING:
		{
			char c = *(value->stringvalue);
			if (!ignoreJson && (c == '[' || c == '{')) {
				lua_pushvalue(L, 3);
				lua_pushstring(L, value->stringvalue);
				int ret = lua_pcall(L, 1, 1, 0);
				if (ret != LUA_OK) {
					return 1;
				}
			} else {
				lua_pushstring(L, value->stringvalue);
			}
		}
		break;
		default:
			lua_pushnil(L);
		break;
	}
	return 0;
}

int lloadcsv(lua_State *L) {
	if (lua_gettop(L) != 3) {
		luaL_error(L, "[csvloader.loadcsv]: need 3 param");
	}
	const char *filename = lua_tostring(L, 1);
	if (!filename) {
		luaL_error(L, "[csvloader.loadcsv]: error filename");
	}

	int ignoreJson = lua_tointeger(L, 2);
	if (!ignoreJson) {
		luaL_checktype(L, 3, LUA_TFUNCTION);
	}

	csv_parse parse;
	int err = Csv_Open(filename, &parse);
	if (err != 0) {
		luaL_error(L, "[csvloader.loadcsv]: open csv [%s] error[%d]", filename, err);
	}
	csv_line firstline, templine;
	Csv_InitLine(&firstline);
	
	err = Csv_ParseOneLine(&parse, &firstline);
	if (err != 0) {
		Csv_FreeLine(&firstline);
		Csv_Close(&parse);
		luaL_error(L, "[csvloader.loadcsv]: open csv [%s] error[%d]", filename, err);
	}

	lua_newtable(L);
	int colindex;
	for (colindex=0; colindex<parse.colnum; colindex++) {
		csv_value *value = Csv_GetLineValue(&firstline, colindex);
		if (value->type != TYPE_STRING) {
			Csv_FreeLine(&firstline);
			Csv_Close(&parse);
			luaL_error(L, "[csvloader.loadcsv]: [%s] head line field [%d] not a string!", filename, colindex);
		}
		lua_pushstring(L, value->stringvalue);
		lua_rawseti(L, -2, colindex+1);
	}

	lua_newtable(L);

	Csv_InitLine(&templine);
	int intc;
	do {
		Csv_ClearLine(&templine);
		intc = Csv_ParseOneLine(&parse, &templine);
		if ((intc == 0 || intc == -1) && templine.valuevec.n > 0) {
			csv_value *keyvalue = Csv_GetLineValue(&templine, 0);
			if (keyvalue->type == TYPE_INT) {
				lua_pushinteger(L, keyvalue->intvalue);
			} else if (keyvalue->type == TYPE_STRING) {
				lua_pushstring(L, keyvalue->stringvalue);
			} else {
				Csv_FreeLine(&firstline);
				Csv_FreeLine(&templine);
				Csv_Close(&parse);
				luaL_error(L, "[csvloader.loadcsv]:[%s]invalid value:[%d], curline:[%d],field:[%d]", filename, keyvalue->type, parse.loadf.curline, 1);
			}
			lua_newtable(L);
			
			for (colindex=0; colindex<templine.valuevec.n; colindex++) {
				csv_value *value = Csv_GetLineValue(&templine, colindex);
				if (value->type != TYPE_NIL) {
					if (pushvalue(L, value, ignoreJson) != 0) {
						Csv_FreeLine(&firstline);
						Csv_FreeLine(&templine);
						Csv_Close(&parse);
						size_t sz = 0;
						const char * error = lua_tolstring(L, -1, &sz);
						luaL_error(L, "[csvloader.loadcsv]: [%s] line:[%d],field[%d]"
							" decode json error :%s",
							filename, parse.loadf.curline, colindex+1, error);
					}

					csv_value *keyvalue = Csv_GetLineValue(&firstline, colindex);
					lua_pushstring(L, keyvalue->stringvalue);
					lua_pushvalue(L, -2);
					lua_rawset(L, -4);

					if (colindex == 0 && strcmp(keyvalue->stringvalue, "id") != 0) {
						lua_pushstring(L, "id");
						lua_pushvalue(L, -2);
						lua_rawset(L, -4);
					}
					lua_pop(L, 1);
				}
			}
			lua_rawset(L, -3);
		}
	} while(intc == 0);

	Csv_FreeLine(&firstline);
	Csv_FreeLine(&templine);
	Csv_Close(&parse);
	return 2;
}

int lmem(lua_State *L) {
	lua_pushinteger(L, Csv_TotalMem());
	return 1;
}

int luaopen_csvloader(lua_State *L) {
	luaL_checkversion(L);

	luaL_Reg l[] = {
		{"loadcsv", lloadcsv},
		{"mem", lmem},
		{NULL, NULL}
	};

	lua_createtable(L, 0, sizeof(l)/sizeof(luaL_Reg)-1);
	luaL_setfuncs(L, l, 0);
	return 1;
}