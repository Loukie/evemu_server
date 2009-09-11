/*
	------------------------------------------------------------------------------------
	LICENSE:
	------------------------------------------------------------------------------------
	This file is part of EVEmu: EVE Online Server Emulator
	Copyright 2006 - 2008 The EVEmu Team
	For the latest information visit http://evemu.mmoforge.org
	------------------------------------------------------------------------------------
	This program is free software; you can redistribute it and/or modify it under
	the terms of the GNU Lesser General Public License as published by the Free Software
	Foundation; either version 2 of the License, or (at your option) any later
	version.

	This program is distributed in the hope that it will be useful, but WITHOUT
	ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
	FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public License for more details.

	You should have received a copy of the GNU Lesser General Public License along with
	this program; if not, write to the Free Software Foundation, Inc., 59 Temple
	Place - Suite 330, Boston, MA 02111-1307, USA, or go to
	http://www.gnu.org/copyleft/lesser.txt.
	------------------------------------------------------------------------------------
	Author:		Zhur
*/


#include "common.h"
#include "DecodeGenerator.h"
#include "../common/logsys.h"
#include "MiscFunctions.h"

#ifndef WIN32
#warning Decoder is not properly freeing old items in the case of object re-use
#endif

ClassDecodeGenerator::ClassDecodeGenerator()
: mItemNumber( 0 ),
  mName( NULL )
{
	AllGenProcRegs(ClassDecodeGenerator);
}

bool ClassDecodeGenerator::Process_elementdef(FILE *into, TiXmlElement *element)
{
	mName = element->Attribute("name");
	if(mName == NULL) {
		_log(COMMON__ERROR, "<element> at line %d is missing the name attribute, skipping.", element->Row());
		return false;
	}

	fprintf(into,
		"bool %s::Decode(%s** in_packet)\n"
		"{\n"
		"	//quick forwarder to avoid making the user cast it if they have a properly typed object\n"
		"	PyRep* packet = *in_packet;\n"
		"	*in_packet = NULL;\n"
		"	return Decode( &packet );\n"
		"}\n"
		"\n"
		"bool %s::Decode(PyRep** in_packet)\n"
		"{\n"
		"	PyRep* packet = *in_packet;\n"
		"	*in_packet = NULL;"
		"\n",
		mName, GetEncodeType(element), mName);

	mItemNumber = 0;

	push("packet");
	
	if(!Recurse(into, element))
		return false;

	fprintf(into,
		"\n"
		"	delete packet;\n"
		"	return true;\n"
		"}\n"
		"\n"
	);

	return true;
}

bool ClassDecodeGenerator::Process_InlineTuple(FILE *into, TiXmlElement *field) {
	//first, we need to know how many elements this tuple has:
	TiXmlNode *i = NULL;
	int count = 0;
	while( (i = field->IterateChildren( i )) ) {
		if(i->Type() == TiXmlNode::ELEMENT)
			count++;
	}

	const char *v = top();
	
	int num = mItemNumber++;
	char iname[16];
	snprintf(iname, sizeof(iname), "tuple%d", num);
	//now we can generate the tuple decl
	fprintf(into, 
		"	if(!%s->IsTuple()) {\n"
		"		_log(NET__PACKET_ERROR, \"Decode %s failed: %s is the wrong type: %%s\", %s->TypeString());\n"
		"		delete packet;\n"
		"		return false;\n"
		"	}\n"
		"	PyTuple *%s = (PyTuple *) %s;\n"
		"	if(%s->items.size() != %d) {\n"
		"		_log(NET__PACKET_ERROR, \"Decode %s failed: %s is the wrong size: expected %d, but got %%lu\", %s->items.size());\n"
		"		delete packet;\n"
		"		return false;\n"
		"	}\n"
		"\n",
		v, 
			mName, iname, v, 
		iname, v, 
		iname, count, 
			mName, iname, count, iname
	);

	//now we need to queue up all the storage locations for the fields
    //need to be backward
	char varname[64];
	for(count--; count >= 0; count--) {
		snprintf(varname, sizeof(varname), "%s->items[%d]", iname, count);
		push(varname);
	}
	
	if(!Recurse(into, field))
		return false;
	
	pop();
	return true;
}

bool ClassDecodeGenerator::Process_InlineList(FILE *into, TiXmlElement *field) {
	//first, we need to know how many elements this tuple has:
	TiXmlNode *i = NULL;
	int count = 0;
	while( (i = field->IterateChildren( i )) ) {
		if(i->Type() != TiXmlNode::ELEMENT)
			continue;	//skip crap we dont care about
		count++;
	}

	const char *v = top();
	
	int num = mItemNumber++;
	char iname[16];
	snprintf(iname, sizeof(iname), "list%d", num);
	//now we can generate the tuple decl
	fprintf(into, 
		"	if(!%s->IsList()) {\n"
		"		_log(NET__PACKET_ERROR, \"Decode %s failed: %s is not a list: %%s\", %s->TypeString());\n"
		"		delete packet;\n"
		"		return false;\n"
		"	}\n"
		"	PyList *%s = (PyList *) %s;\n"
		"	if(%s->items.size() != %d) {\n"
		"		_log(NET__PACKET_ERROR, \"Decode %s failed: %s is the wrong size: expected %d, but got %%lu\", %s->items.size());\n"
		"		delete packet;\n"
		"		return false;\n"
		"	}\n"
		"\n",
		v, 
			mName, iname, v, 
		iname, v, 
		iname, count, 
			mName, iname, count, iname
	);

	//now we need to queue up all the storage locations for the fields
    //need to be backward
	char varname[64];
	for(count--; count >= 0; count--) {
		snprintf(varname, sizeof(varname), "%s->items[%d]", iname, count);
		push(varname);
	}
	
	if(!Recurse(into, field))
		return false;
	
	pop();
	return true;
}

/*
 *this function could be improved quite a bit by not parsing and checking the
 *IDEntry elements over and over again
 *
*/
bool ClassDecodeGenerator::Process_InlineDict(FILE *into, TiXmlElement *field) {
	TiXmlNode *i = NULL;
	
	int num = mItemNumber++;
	char iname[16];
	snprintf(iname, sizeof(iname), "dict%d", num);
	const char *v = top();

	//make sure its a dict
	fprintf(into, 
		"	if(!%s->IsDict()) {\n"
		"		_log(NET__PACKET_ERROR, \"Decode %s failed: %s is the wrong type: %%s\", %s->TypeString());\n"
		"		delete packet;\n"
		"		return false;\n"
		"	}"
		"\n",
		v, mName, iname, v
	);

	bool empty = true;
	//now generate the "found" flags for each expected element.
	while( (i = field->IterateChildren( i )) ) {
		if(i->Type() != TiXmlNode::ELEMENT)
			continue;	//skip crap we dont care about
		TiXmlElement *ele = i->ToElement();
		//we only handle IDEntry elements
		if(std::string("IDEntry") != ele->Value()) {
			_log(COMMON__ERROR, "non-IDEntry in <InlineDict> at line %d, ignoring.", ele->Row());
			continue;
		}
		TiXmlElement *val = ele->FirstChildElement();
		if(val == NULL) {
			_log(COMMON__ERROR, "<IDEntry> at line %d lacks a value element", ele->Row());
			return false;
		}
		const char *vname = val->Attribute("name");
		if(vname == NULL) {
			_log(COMMON__ERROR, "<IDEntry>'s value element at line %d lacks a name", val->Row());
			return false;
		}
		fprintf(into,
			"\tbool %s_%s = false;\n", iname, vname);
		empty = false;
	}

	if(empty) {
		fprintf(into, "	//%s is empty from our perspective, not enforcing though.\n", iname);
	} else {
		//setup the loop...
		fprintf(into, 
			"	PyDict *%s = (PyDict *) %s;\n"
			"	\n"
			"	PyDict::iterator %s_cur, %s_end;\n"
			"	%s_cur = %s->items.begin();\n"
			"	%s_end = %s->items.end();\n"
			"	for(; %s_cur != %s_end; %s_cur++) {\n"
			"		PyRep *key__ = %s_cur->first;\n"
			"		if(!key__->IsString()) {\n"
			"			_log(NET__PACKET_ERROR, \"Decode %s failed: a key in %s is the wrong type: %%s\", key__->TypeString());\n"
			"			delete packet;\n"
			"			return false;\n"
			"		}\n"
			"		PyString *key_string__ = (PyString *) key__;\n"
			"		\n",
			iname, v, iname, iname, iname, iname, iname, iname, iname, iname, iname, iname, mName, iname
		);

		while((i = field->IterateChildren( i )) ) {
			if(i->Type() == TiXmlNode::COMMENT) {
				TiXmlComment *com = i->ToComment();
				fprintf(into, "\t/* %s */\n", com->Value());
				continue;
			}
			if(i->Type() != TiXmlNode::ELEMENT)
				continue;	//skip crap we dont care about
	
			TiXmlElement *ele = i->ToElement();
			//we only handle IDEntry elements
			if(std::string("IDEntry") != ele->Value()) {
				_log(COMMON__ERROR, "non-IDEntry in <InlineDict> at line %d, ignoring.", ele->Row());
				continue;
			}
			const char *key = ele->Attribute("key");
			if(key == NULL) {
				_log(COMMON__ERROR, "<IDEntry> at line %d lacks a key attribute", ele->Row());
				return false;
			}
			TiXmlElement *val = ele->FirstChildElement();
			if(val == NULL) {
				_log(COMMON__ERROR, "<IDEntry> at line %d lacks a value element", ele->Row());
				return false;
			}
			const char *vname = val->Attribute("name");
			if(vname == NULL) {
				_log(COMMON__ERROR, "<IDEntry>'s value element at line %d lacks a name", val->Row());
				return false;
			}
	
			//conditional prefix...
			fprintf(into, 
				"		if(*key_string__ == \"%s\") {\n"
				"			%s_%s = true;\n",
				key, iname, vname );
			
			//now process the data part, putting the value into `varname`
            //TODO: figure out a reasonable way to fix the indention here...
			char vvname[64];
			snprintf(vvname, sizeof(vvname), "%s_cur->second", iname);
			push(vvname);
			
			if(!Recurse(into, ele, 1))
				return false;
			//fixed suffix...
			fprintf(into, 
				"		} else\n");
		}

		if(field->Attribute("soft") == NULL || field->Attribute("soft") != std::string("true")) {
			fprintf(into, 
				"		{\n"
				"			_log(NET__PACKET_ERROR, \"Decode %s failed: Unknown key string '%%s' in %s\", key_string__->content());\n"
				"			delete packet;\n"
				"			return false;\n"
				"		}\n"
				"	}\n"
				"	\n",
				mName, iname);
		} else {
			fprintf(into, 
				"		{ /* do nothing, soft dict */ }\n"
				"	}\n"
				"	\n" );
		}
		
		//finally, check the "found" flags for each expected element.
		while( (i = field->IterateChildren( i )) ) {
			if(i->Type() != TiXmlNode::ELEMENT)
				continue;	//skip crap we dont care about
			TiXmlElement *ele = i->ToElement();
			//we only handle IDEntry elements
			if(std::string("IDEntry") != ele->Value()) {
				_log(COMMON__ERROR, "non-IDEntry in <InlineDict> at line %d, ignoring.", ele->Row());
				continue;
			}
			TiXmlElement *val = ele->FirstChildElement();
			if(val == NULL) {
				_log(COMMON__ERROR, "<IDEntry> at line %d lacks a value element", ele->Row());
				return false;
			}
			const char *vname = val->Attribute("name");
			if(vname == NULL) {
				_log(COMMON__ERROR, "<IDEntry>'s value element at line %d lacks a name", val->Row());
				return false;
			}
			
			fprintf(into, 
				"	if(!%s_%s) {\n"
				"		_log(NET__PACKET_ERROR, \"Decode %s failed: Missing dict entry for '%s' in %s\");\n"
				"		delete packet;\n"
				"		return false;\n"
				"	}\n"
				"	\n",
				iname, vname, mName, vname, iname);
		}
	}
	

	
	pop();
	return true;
}

bool ClassDecodeGenerator::Process_InlineSubStream(FILE *into, TiXmlElement *field) {
	char iname[16];
	int num = mItemNumber++;
	snprintf(iname, sizeof(iname), "ss_%d", num);
	const char *v = top();

	//make sure its a substream
	
	fprintf(into, 
		"	if(!%s->IsSubStream()) {\n"
		"		_log(NET__PACKET_ERROR, \"Decode %s failed: %s is not a substream: %%s\", %s->TypeString());\n"
		"		delete packet;\n"
		"		return false;\n"
		"	}"
		"	\n"
		"	PySubStream *%s = (PySubStream *) %s;\n"
		"	//make sure its decoded\n"
		"	%s->DecodeData();\n"
		"	if(%s->decoded == NULL) {\n"
		"		_log(NET__PACKET_ERROR, \"Decode %s failed: Unable to decode %s\");\n"
		"		delete packet;\n"
		"		return false;\n"
		"	}\n"
		"	\n",
		v, mName, iname, v, iname, v, iname, iname, mName, iname
	);
	char ssname[32];
	snprintf(ssname, sizeof(ssname), "%s->decoded", iname);
	//Decode the sub-element
	push(ssname);
	if(!Recurse(into, field, 1))
		return false;
	
	pop();
	return true;
}

bool ClassDecodeGenerator::Process_InlineSubStruct(FILE *into, TiXmlElement *field) {
	char iname[16];
	int num = mItemNumber++;
	snprintf(iname, sizeof(iname), "ss_%d", num);
	const char *v = top();

	//make sure its a substream
	
	fprintf(into, 
		"	if(!%s->IsSubStruct()) {\n"
		"		_log(NET__PACKET_ERROR, \"Decode %s failed: %s is not a substruct: %%s\", %s->TypeString());\n"
		"		delete packet;\n"
		"		return false;\n"
		"	}"
		"	\n"
		"	PySubStruct *%s = (PySubStruct *) %s;\n"
		"	\n",
		v, 
			mName, iname, v, 
		iname, v
	);
	char ssname[32];
	snprintf(ssname, sizeof(ssname), "%s->sub", iname);
	//Decode the sub-element
	push(ssname);
	if(!Recurse(into, field, 1))
		return false;
	
	pop();
	return true;
}

bool ClassDecodeGenerator::Process_strdict(FILE *into, TiXmlElement *field) {
	const char *name = field->Attribute("name");
	if(name == NULL) {
		_log(COMMON__ERROR, "field at line %d is missing the name attribute, skipping.", field->Row());
		return false;
	}
	
	char iname[16];
	int num = mItemNumber++;
	snprintf(iname, sizeof(iname), "dict_%d", num);
	const char *v = top();
	
	fprintf(into, 
		"	if(!%s->IsDict()) {\n"
		"		_log(NET__PACKET_ERROR, \"Decode %s failed: %s is not a dict: %%s\", %s->TypeString());\n"
		"		delete packet;\n"
		"		return false;\n"
		"	}\n"
		"	%s.clear();\n"
		"	PyDict *%s = (PyDict *) %s;\n"
		"	PyDict::iterator %s_cur, %s_end;\n"
		"	%s_cur = %s->items.begin();\n"
		"	%s_end = %s->items.end();\n"
		"	int %s_index;\n"
		"	for(%s_index = 0; %s_cur != %s_end; %s_cur++, %s_index++) {\n"
		"		if(!%s_cur->first->IsString()) {\n"
		"			_log(NET__PACKET_ERROR, \"Decode %s failed: Key %%d in dict %s is not a string: %%s\", %s_index, %s_cur->first->TypeString());\n"
		"			delete packet;\n"
		"			return false;\n"
		"		}\n"
		"		PyString *k = (PyString *) %s_cur->first;\n"
		"		%s[k->content()] = %s_cur->second->Clone();\n"
		"	}\n"
		"	\n",
		v,
			mName, name, v,
		name,
		iname, v,
		name, name,
		name, iname, 
		name, iname,
		name,
		name, name, name, name, name,
			name, 
				mName, name, name, name, 
			name,
			name, name
	);
	
	pop();
	return true;
}

bool ClassDecodeGenerator::Process_intdict(FILE *into, TiXmlElement *field) {
	const char *name = field->Attribute("name");
	if(name == NULL) {
		_log(COMMON__ERROR, "field at line %d is missing the name attribute, skipping.", field->Row());
		return false;
	}
	
	char iname[16];
	int num = mItemNumber++;
	snprintf(iname, sizeof(iname), "dict_%d", num);
	const char *v = top();
	
	fprintf(into, 
		"	if(!%s->IsDict()) {\n"
		"		_log(NET__PACKET_ERROR, \"Decode %s failed: %s is not a dict: %%s\", %s->TypeString());\n"
		"		delete packet;\n"
		"		return false;\n"
		"	}\n"
		"	%s.clear();\n"
		"	PyDict *%s = (PyDict *) %s;\n"
		"	PyDict::iterator %s_cur, %s_end;\n"
		"	%s_cur = %s->items.begin();\n"
		"	%s_end = %s->items.end();\n"
		"	int %s_index;\n"
		"	for(%s_index = 0; %s_cur != %s_end; %s_cur++, %s_index++) {\n"
		"		if(!%s_cur->first->IsInt()) {\n"
		"			_log(NET__PACKET_ERROR, \"Decode %s failed: Key %%d in dict %s is not an integer: %%s\", %s_index, %s_cur->first->TypeString());\n"
		"			delete packet;\n"
		"			return false;\n"
		"		}\n"
		"		PyInt *k = (PyInt *) %s_cur->first;\n"
		"		%s[k->value] = %s_cur->second->Clone();\n"
		"	}\n"
		"	\n",
		v,
			mName, name, v,
		name,
		iname, v,
		name, name,
		name, iname, 
		name, iname,
		name,
		name, name, name, name, name,
			name,
				mName, name, name, name,
			name,
			name, name
	);

	pop();
	return true;
}

bool ClassDecodeGenerator::Process_primdict(FILE *into, TiXmlElement *field) {
	const char *name = field->Attribute("name");
	if(name == NULL) {
		_log(COMMON__ERROR, "field at line %d is missing the name attribute, skipping.", field->Row());
		return false;
	}
	const char *key = field->Attribute("key");
	if(key == NULL) {
		_log(COMMON__ERROR, "field at line %d is missing the key attribute, skipping.", field->Row());
		return false;
	}
	const char *pykey = field->Attribute("pykey");
	if(pykey == NULL) {
		_log(COMMON__ERROR, "field at line %d is missing the pykey attribute, skipping.", field->Row());
		return false;
	}
	const char *value = field->Attribute("value");
	if(value == NULL) {
		_log(COMMON__ERROR, "field at line %d is missing the value attribute, skipping.", field->Row());
		return false;
	}
	const char *pyvalue = field->Attribute("pyvalue");
	if(pyvalue == NULL) {
		_log(COMMON__ERROR, "field at line %d is missing the pyvalue attribute, skipping.", field->Row());
		return false;
	}
	
	char iname[16];
	int num = mItemNumber++;
	snprintf(iname, sizeof(iname), "dict_%d", num);
	const char *v = top();
	
	fprintf(into, 
		"	if(!%s->IsDict()) {\n"
		"		_log(NET__PACKET_ERROR, \"Decode %s failed: %s is not a dict: %%s\", %s->TypeString());\n"
		"		delete packet;\n"
		"		return false;\n"
		"	}\n"
		"	%s.clear();\n"
		"	PyDict *%s = (PyDict *) %s;\n"
		"	PyDict::iterator %s_cur, %s_end;\n"
		"	%s_cur = %s->items.begin();\n"
		"	%s_end = %s->items.end();\n"
		"	int %s_index;\n"
		"	for(%s_index = 0; %s_cur != %s_end; %s_cur++, %s_index++) {\n"
		"		if(!%s_cur->first->Is%s()) {\n"
		"			_log(NET__PACKET_ERROR, \"Decode %s failed: Key %%d in dict %s is not %s: %%s\", %s_index, %s_cur->first->TypeString());\n"
		"			delete packet;\n"
		"			return false;\n"
		"		}\n"
		"		if(!%s_cur->second->Is%s()) {\n"
		"			_log(NET__PACKET_ERROR, \"Decode %s failed: Value %%d in dict %s is not %s: %%s\", %s_index, %s_cur->second->TypeString());\n"
		"			delete packet;\n"
		"			return false;\n"
		"		}\n"
		"		Py%s *k = (Py%s *) %s_cur->first;\n"
		"		Py%s *v = (Py%s *) %s_cur->second;\n"
		"		%s[k->value] = v->value;\n"
		"	}\n"
		"	\n",
		v,
			mName, name, v,
		name,
		iname, v,
		name, name,
		name, iname, 
		name, iname,
		name,
		name, name, name, name, name,
			name, pykey, 
				mName, name, pykey, name, name, 
			name, pyvalue, 
				mName, name, pyvalue, name, name, 
			pykey, pykey, name,
			pyvalue, pyvalue, name,
			name
	);
	
	pop();
	return true;
}

bool ClassDecodeGenerator::Process_strlist(FILE *into, TiXmlElement *field) {
	const char *name = field->Attribute("name");
	if(name == NULL) {
		_log(COMMON__ERROR, "field at line %d is missing the name attribute, skipping.", field->Row());
		return false;
	}
	
	char iname[16];
	int num = mItemNumber++;
	snprintf(iname, sizeof(iname), "list_%d", num);
	const char *v = top();

	
	//make sure its a dict
	fprintf(into, 
		"	if(!%s->IsList()) {\n"
		"		_log(NET__PACKET_ERROR, \"Decode %s failed: %s is not a list: %%s\", %s->TypeString());\n"
		"		delete packet;\n"
		"		return false;\n"
		"	}\n"
		"	%s.clear();\n"
		"	PyList *%s = (PyList *) %s;\n"
		"	PyList::iterator %s_cur, %s_end;\n"
		"	%s_cur = %s->items.begin();\n"
		"	%s_end = %s->items.end();\n"
		"	int %s_index;\n"
		"	for(%s_index = 0; %s_cur != %s_end; %s_cur++, %s_index++) {\n"
		"		if(!(*%s_cur)->IsString()) {\n"
		"			_log(NET__PACKET_ERROR, \"Decode %s failed: Element %%d in list %s is not a string: %%s\", %s_index, (*%s_cur)->TypeString());\n"
		"			delete packet;\n"
		"			return false;\n"
		"		}\n"
		"		PyString *t = (PyString *) (*%s_cur);\n"
		"		%s.push_back(t->content());\n"
		"	}\n"
		"\n",
		v, mName, name, v, name,
		iname, v,
		name, name, name, iname, name, iname,
		name, name, name, name, name, name,
		name, mName, name, name, name, name, name
	);
	
	pop();
	return true;
}

bool ClassDecodeGenerator::Process_intlist(FILE *into, TiXmlElement *field) {
	const char *name = field->Attribute("name");
	if(name == NULL) {
		_log(COMMON__ERROR, "field at line %d is missing the name attribute, skipping.", field->Row());
		return false;
	}
	
	char iname[16];
	int num = mItemNumber++;
	snprintf(iname, sizeof(iname), "list_%d", num);
	const char *v = top();

	
	//make sure its a dict
	fprintf(into, 
		"	if(!%s->IsList()) {\n"
		"		_log(NET__PACKET_ERROR, \"Decode %s failed: %s is not a list: %%s\", %s->TypeString());\n"
		"		delete packet;\n"
		"		return false;\n"
		"	}\n"
		"	%s.clear();\n"
		"	PyList *%s = (PyList *) %s;\n"
		"	PyList::iterator %s_cur, %s_end;\n"
		"	%s_cur = %s->items.begin();\n"
		"	%s_end = %s->items.end();\n"
		"	int %s_index;\n"
		"	for(%s_index = 0; %s_cur != %s_end; %s_cur++, %s_index++) {\n"
		"		if(!(*%s_cur)->IsInt()) {\n"
		"			_log(NET__PACKET_ERROR, \"Decode %s failed: Element %%d in list %s is not an integer: %%s\", %s_index, (*%s_cur)->TypeString());\n"
		"			delete packet;\n"
		"			return false;\n"
		"		}\n"
		"		PyInt *t = (PyInt *) (*%s_cur);\n"
		"		%s.push_back(t->value);\n"
		"	}\n"
		"\n",
		v, mName, name, v, name,
		iname, v,
		name, name, name, iname, name, iname,
		name, name, name, name, name, name,
		name, mName, name, name, name, name,
		name
	);

	pop();
	return true;
}

bool ClassDecodeGenerator::Process_int64list(FILE *into, TiXmlElement *field) {
	const char *name = field->Attribute("name");
	if(name == NULL) {
		_log(COMMON__ERROR, "field at line %d is missing the name attribute, skipping.", field->Row());
		return false;
	}
	
	char iname[16];
	int num = mItemNumber++;
	snprintf(iname, sizeof(iname), "list_%d", num);
	const char *v = top();

	
	//make sure its a dict
	fprintf(into, 
		"	if(!%s->IsList()) {\n"
		"		_log(NET__PACKET_ERROR, \"Decode %s failed: %s is not a list: %%s\", %s->TypeString());\n"
		"		delete packet;\n"
		"		return false;\n"
		"	}\n"
		"	%s.clear();\n"
		"	PyList *%s = (PyList *) %s;\n"
		"	PyList::iterator %s_cur, %s_end;\n"
		"	%s_cur = %s->items.begin();\n"
		"	%s_end = %s->items.end();\n"
		"	int %s_index;\n"
		"	for(%s_index = 0; %s_cur != %s_end; %s_cur++, %s_index++) {\n"
		"		if( (*%s_cur)->IsLong() ) {\n"
		"			PyLong *t = (PyLong *) (*%s_cur);\n"
		"			%s.push_back(t->value);\n"
		"		} else if( (*%s_cur)->IsInt() ) {\n"
		"			PyInt *t = (PyInt *) (*%s_cur);\n"
		"			%s.push_back(t->value);\n"
		"		} else {\n"
		"			_log(NET__PACKET_ERROR, \"Decode %s failed: Element %%d in list %s is not a long integer: %%s\", %s_index, (*%s_cur)->TypeString());\n"
		"			delete packet;\n"
		"			return false;\n"
		"		}\n"
		"	}\n"
		"\n",
		v,
			mName, name, v,
		name,
		iname, v,
		name, name,
		name, iname,
		name, iname,
		name,
		name, name, name, name, name,
		name,
			name,
			name,
		name,
			name,
			name,
			mName, name, name, name
	);
	
	pop();
	return true;
}

bool ClassDecodeGenerator::Process_element(FILE *into, TiXmlElement *field) {
	const char *name = field->Attribute("name");
	if(name == NULL) {
		_log(COMMON__ERROR, "field at line %d is missing the name attribute, skipping.", field->Row());
		return false;
	}
	const char *type = field->Attribute("type");
	if(type == NULL) {
		_log(COMMON__ERROR, "field at line %d is missing the type attribute, skipping.", field->Row());
		return false;
	}

	char iname[16];
	int num = mItemNumber++;
	snprintf(iname, sizeof(iname), "rep_%d", num);
	
	const char *v = top();
	fprintf(into, 
		"	PyRep *%s = %s;\n"	//in case it is typed for some reason.
		"	%s = NULL;\n"	//consume it
		"	if(!%s.Decode(&%s)) {\n"
		"		_log(NET__PACKET_ERROR, \"Decode %s failed: unable to decode element %s\");\n"
		"		delete packet;\n"
		"		return false;\n"
		"	}\n"
		"	\n",
		iname, v,
		v,
		name, iname,
			mName, name
		);
	
	pop();
	return true;
}

bool ClassDecodeGenerator::Process_elementptr(FILE *into, TiXmlElement *field) {
	const char *name = field->Attribute("name");
	if(name == NULL) {
		_log(COMMON__ERROR, "field at line %d is missing the name attribute, skipping.", field->Row());
		return false;
	}
	const char *type = field->Attribute("type");
	if(type == NULL) {
		_log(COMMON__ERROR, "field at line %d is missing the type attribute, skipping.", field->Row());
		return false;
	}

	char iname[16];
	int num = mItemNumber++;
	snprintf(iname, sizeof(iname), "rep_%d", num);
	
	const char *v = top();
	fprintf(into, 
		"	PyRep *%s = %s;\n"	//in case it is typed for some reason.
		"	%s = NULL;\n"
		"	delete %s;\n"
		"	%s = new %s;\n"
		"	if(!%s->Decode(&%s)) {\n"
		"		_log(NET__PACKET_ERROR, \"Decode %s failed: unable to decode element %s\");\n"
		"		delete packet;\n"
		"		return false;\n"
		"	}\n"
		"	\n",
		iname, v,
		v,
		name,
		name, type,
		name, iname,
			mName, name
		);
	
	pop();
	return true;
}

bool ClassDecodeGenerator::Process_none(FILE *into, TiXmlElement *field) {
	
	const char *v = top();
	fprintf(into, 
		"	if(!%s->IsNone()) {\n"
		"		_log(NET__PACKET_ERROR, \"Decode %s failed: expecting a None but got a %%s\", %s->TypeString());\n"
		"		delete packet;\n"
		"		return false;\n"
		"	}\n"
		"	\n",
		v, mName, v
		);
	
	pop();
	return true;
}

bool ClassDecodeGenerator::Process_object(FILE *into, TiXmlElement *field) {
	const char *type = field->Attribute("type");
	if(type == NULL) {
		_log(COMMON__ERROR, "object at line %d is missing the type attribute, skipping.", field->Row());
		return false;
	}

	char iname[16];
	int num = mItemNumber++;
	snprintf(iname, sizeof(iname), "obj_%d", num);
	const char *v = top();

	//make sure its a substream
	
	fprintf(into, 
		"	if(!%s->IsObject()) {\n"
		"		_log(NET__PACKET_ERROR, \"Decode %s failed: %s is the wrong type: %%s\", %s->TypeString());\n"
		"		delete packet;\n"
		"		return false;\n"
		"	}\n"
		"	PyObject *%s = (PyObject *) %s;\n"
		"	\n"
		"	if(%s->type != \"%s\") {\n"
		"		_log(NET__PACKET_ERROR, \"Decode %s failed: %s is the wrong object type. Expected '%s', got '%%s'\", %s->type.c_str());\n"
		"		delete packet;\n"
		"		return false;\n"
		"	}\n"
		"	\n",
		v, mName, iname, v, iname, v, iname, type, mName, iname  , type, iname
	);
	char ssname[32];
	snprintf(ssname, sizeof(ssname), "%s->arguments", iname);
	//Decode the sub-element
	push(ssname);
	if(!Recurse(into, field, 1))
		return false;
	
	pop();
	return true;
}

bool ClassDecodeGenerator::Process_object_ex(FILE *into, TiXmlElement *field)
{
	const char *name = field->Attribute( "name" );
	if( name == NULL )
	{
		_log( COMMON__ERROR, "field at line %d is missing the name attribute, skipping.", field->Row() );
		return false;
	}
	const char *type = field->Attribute("type");
	if( type == NULL )
	{
		_log( COMMON__ERROR, "field at line %d is missing the type attribute.", field->Row() );
		return false;
	}
	bool optional = false;
	const char *optional_str = field->Attribute( "optional" );
	if( optional_str != NULL )
		optional = atobool( optional_str );

	const char *v = top();
	if( optional )
	{
		fprintf( into,
			"    if(%s->IsNone())\n"
			"    {\n"
			"        PySafeDecRef( %s );\n"
			"        %s = NULL;\n"
			"    }\n"
			"    else\n",
			v,
				name,
				name
		);
	}

	fprintf( into,
		"    if(%s->IsObjectEx())\n"
		"    {\n"
		"        PySafeDecRef( %s );\n"
		"        %s = (%s *)&%s->AsObjectEx();\n"
		"        %s = NULL;\n"
		"    }\n"
		"    else\n"
		"    {\n"
		"		_log(NET__PACKET_ERROR, \"Decode %s failed: %s is the wrong type: %%s\", %s->TypeString());\n"
		"		delete packet;\n"
		"		return false;\n"
		"	}\n",
		v,
			name,
			name, type, v,
			v,
			mName, name, v
	);

	pop();
	return true;
}

bool ClassDecodeGenerator::Process_buffer(FILE *into, TiXmlElement *field) {
	const char *name = field->Attribute("name");
	if(name == NULL) {
		_log(COMMON__ERROR, "field at line %d is missing the name attribute, skipping.", field->Row());
		return false;
	}
	
	const char *v = top();
	fprintf(into, 
		"	if(%s->IsBuffer()) {\n"
		"		%s = (PyBuffer *) %s;\n"
		"		%s = NULL;\n"
		"	} else if(%s->IsString()) {\n"
		"		%s = new PyBuffer( %s->AsString() );\n"
		"	} else {\n"
		"		_log(NET__PACKET_ERROR, \"Decode %s failed: %s is not a buffer: %%s\", %s->TypeString());\n"
		"		delete packet;\n"
		"		return false;\n"
		"	}\n"
		"	\n",
			v,
				name, v, 
				v,
			v,
				name, v,
		
				mName, name, v
		);
	
	pop();
	return true;
}

bool ClassDecodeGenerator::Process_raw(FILE *into, TiXmlElement *field) {
	const char *name = field->Attribute("name");
	if(name == NULL) {
		_log(COMMON__ERROR, "field at line %d is missing the name attribute, skipping.", field->Row());
		return false;
	}
	
	const char *v = top();
	fprintf(into, 
		"	delete %s;\n"
		"	%s = %s;\n"
		"	%s = NULL;\n"
		"	\n",
		name,
		name, v,
		v
		);
	
	pop();
	return true;
}

bool ClassDecodeGenerator::Process_list(FILE *into, TiXmlElement *field) {
	const char *name = field->Attribute("name");
	if(name == NULL) {
		_log(COMMON__ERROR, "field at line %d is missing the name attribute, skipping.", field->Row());
		return false;
	}
	const char *v = top();
	
	//this should be done better:
	const char *optional = field->Attribute("optional");
	bool is_optional = false;
	if(optional != NULL && std::string("true") == optional)
		is_optional = true;
	if(is_optional) {
		fprintf(into, 
			"	if(%s->IsNone()) {\n"
			"		%s.clear();\n"
			"	} else {\n",
			v,
				name);
		is_optional = true;
	}
	
	fprintf(into, 
		"	if(!%s->IsList()) {\n"
		"		_log(NET__PACKET_ERROR, \"Decode %s failed: %s is not a list: %%s\", %s->TypeString());\n"
		"		delete packet;\n"
		"		return false;\n"
		"	}\n"
		"	PyList *list_%s = (PyList *) %s;"
		"	%s.items = list_%s->items;\n"
		"	list_%s->items.clear();\n"
		"	\n",
		v, mName, name, v, 
		name, v,
		name, name,
		name
		);
	
	if(is_optional) {
		fprintf(into, "	}\n");
	}
	
	pop();
	return true;
}

bool ClassDecodeGenerator::Process_tuple(FILE *into, TiXmlElement *field) {
	const char *name = field->Attribute("name");
	if(name == NULL) {
		_log(COMMON__ERROR, "field at line %d is missing the name attribute, skipping.", field->Row());
		return false;
	}
	
	bool is_optional = false;
	const char *optional = field->Attribute("optional");
	if(optional != NULL)
		is_optional = atobool( optional );

	const char *v = top();
	if(is_optional) {
		fprintf(into, 
			"	if(%s->IsNone()) {\n"
			"		PySafeDecRef( %s );\n"
			"		%s = NULL;\n"
			"	} else {\n",
			v,
				name,
				name
		);
	}
	
	fprintf(into, 
		"	if(!%s->IsTuple()) {\n"
		"		_log(NET__PACKET_ERROR, \"Decode %s failed: %s is not a tuple: %%s\", %s->TypeString());\n"
		"		delete packet;\n"
		"		return false;\n"
		"	}\n"
		"	%s = (PyTuple *) %s;\n"
		"	%s = NULL;\n"
		"	\n",
		v,
			mName, name, v,
		name, v,
		v
		);
	
	if(is_optional) {
		fprintf(into, "	}\n");
	}
	
	pop();
	return true;
}

bool ClassDecodeGenerator::Process_dict(FILE *into, TiXmlElement *field) {
	const char *name = field->Attribute("name");
	if(name == NULL) {
		_log(COMMON__ERROR, "field at line %d is missing the name attribute, skipping.", field->Row());
		return false;
	}
	
	const char *v = top();
	
	//this should be done better:
	const char *optional = field->Attribute("optional");
	bool is_optional = false;
	if(optional != NULL && std::string("true") == optional)
		is_optional = true;
	if(is_optional) {
		fprintf(into, 
			"	if(%s->IsNone()) {\n"
			"		%s.clear();\n"
			"	} else {\n",
			v,
				name);
		is_optional = true;
	}
	
	fprintf(into, 
		"	if(!%s->IsDict()) {\n"
		"		_log(NET__PACKET_ERROR, \"Decode %s failed: %s is not a dict: %%s\", %s->TypeString());\n"
		"		delete packet;\n"
		"		return false;\n"
		"	}\n"
		"	PyDict *list_%s = (PyDict *) %s;"
		"	%s.items = list_%s->items;\n"
		"	list_%s->items.clear();\n"
		"	\n",
		v, mName, name, v, 
		name, v,
		name, name,
		name
		);
	
	if(is_optional) {
		fprintf(into, "	}\n");
	}
	
	pop();
	return true;
}

bool ClassDecodeGenerator::Process_bool(FILE *into, TiXmlElement *field) {
	const char *name = field->Attribute("name");
	if(name == NULL) {
		_log(COMMON__ERROR, "field at line %d is missing the name attribute, skipping.", field->Row());
		return false;
	}
	
	char iname[16];
	int num = mItemNumber++;
	snprintf(iname, sizeof(iname), "bool_%d", num);
	
	const char *v = top();
	
	//this should be done better:
	const char *none_marker = field->Attribute("none_marker");
	bool is_optional = false;
	if(none_marker != NULL) {
		fprintf(into, 
			"	if(%s->IsNone()) {\n"
			"		%s = %s;\n"
			"	} else {\n",
			v,
				name, none_marker);
		is_optional = true;
	}
	
	if(field->Attribute("soft") == NULL || field->Attribute("soft") != std::string("true")) {
		fprintf(into, 
			"	if(!%s->IsBool()) {\n"
			"		_log(NET__PACKET_ERROR, \"Decode %s failed: %s is not a boolean: %%s\", %s->TypeString());\n"
			"		delete packet;\n"
			"		return false;\n"
			"	}\n"
			"	PyBool *%s = (PyBool *) %s;\n"
			"	%s = %s->value;\n"
			"",
			v, mName, name, v, iname, v, name, iname
			);
	} else {
			fprintf(into, 
			"	if(%s->IsBool()) {\n"
			"		PyBool *%s = (PyBool *) %s;\n"
			"		%s = %s->value;\n"
			"	} else if(%s->IsInt()) {\n"
			"		PyInt *%s = (PyInt *) %s;\n"
			"		%s = (%s->value != 0);\n"
			"	} else {\n"
			"		_log(NET__PACKET_ERROR, \"Decode %s failed: %s is not a boolean (or int): %%s\", %s->TypeString());\n"
			"		delete packet;\n"
			"		return false;\n"
			"	}\n"
			"",
			v, 
				iname, v, 
				name, iname,
			v, 
				iname, v, 
				name, iname,
				mName, name, v
			);
	}
	
	if(is_optional) {
		fprintf(into, "	}\n");
	}
	
	pop();
	return true;
}

bool ClassDecodeGenerator::Process_int(FILE *into, TiXmlElement *field) {
	const char *name = field->Attribute("name");
	if(name == NULL) {
		_log(COMMON__ERROR, "field at line %d is missing the name attribute, skipping.", field->Row());
		return false;
	}
	
	const char *v = top();
	
	//this should be done better:
	const char *none_marker = field->Attribute("none_marker");
	bool is_optional = false;
	if(none_marker != NULL) {
		fprintf(into, 
			"	if(%s->IsNone()) {\n"
			"		%s = %s;\n"
			"	} else {\n",
			v,
				name, none_marker);
		is_optional = true;
	}
	
	char iname[16];
	int num = mItemNumber++;
	snprintf(iname, sizeof(iname), "int_%d", num);
	
	fprintf(into, 
		"	if(!%s->IsInt()) {\n"
		"		_log(NET__PACKET_ERROR, \"Decode %s failed: %s is not an int: %%s\", %s->TypeString());\n"
		"		delete packet;\n"
		"		return false;\n"
		"	}\n"
		"	PyInt *%s = (PyInt *) %s;\n"
		"	%s = %s->value;\n"
		"",
		v, mName, name, v, iname, v,
		name, iname
		);

	if(is_optional) {
		fprintf(into, "	}\n");
	}
	
	pop();
	return true;
}

bool ClassDecodeGenerator::Process_int64(FILE *into, TiXmlElement *field) {
	const char *name = field->Attribute("name");
	if(name == NULL) {
		_log(COMMON__ERROR, "field at line %d is missing the name attribute, skipping.", field->Row());
		return false;
	}
	const char *v = top();
	
	//this should be done better:
	const char *none_marker = field->Attribute("none_marker");
	bool is_optional = false;
	if(none_marker != NULL) {
		fprintf(into, 
			"	if(%s->IsNone()) {\n"
			"		%s = %s;\n"
			"	} else {\n",
			v,
				name, none_marker);
		is_optional = true;
	}
	
	char iname[16];
	int num = mItemNumber++;
	snprintf(iname, sizeof(iname), "int64_%d", num);
	
	fprintf(into, 
		"	if( %s->IsLong() ) {\n"
		"		PyLong *%s = (PyLong *) %s;\n"
		"		%s = %s->value;\n"
		"	} else if( %s->IsInt() ) {\n"
		"		PyInt *%s = (PyInt *) %s;\n"
		"		%s = %s->value;\n"
		"	} else {\n"
		"		_log(NET__PACKET_ERROR, \"Decode %s failed: %s is not a long int: %%s\", %s->TypeString());\n"
		"		delete packet;\n"
		"		return false;\n"
		"	}\n",
		v,
			iname, v,
			name, iname,
		v,
			iname, v,
			name, iname,
			mName, name, v
		);

	if(is_optional) {
		fprintf(into, "	}\n");
	}
	
	pop();
	return true;
}

bool ClassDecodeGenerator::Process_string(FILE *into, TiXmlElement *field) {
	const char *name = field->Attribute("name");
	if(name == NULL) {
		_log(COMMON__ERROR, "field at line %d is missing the name attribute, skipping.", field->Row());
		return false;
	}
	const char *type1 = field->Attribute("type1");
	
	const char *v = top();

	//this should be done better:
	const char *none_marker = field->Attribute("none_marker");
	bool is_optional = false;
	if(none_marker != NULL) {
		fprintf(into, 
			"	if(%s->IsNone()) {\n"
			"		%s = \"%s\";\n"
			"	} else {\n",
			v,
				name, none_marker);
		is_optional = true;
	}
	
	char iname[16];
	int num = mItemNumber++;
	snprintf(iname, sizeof(iname), "string_%d", num);
	
	
	fprintf(into, 
		"	if(!%s->IsString()) {\n"
		"		_log(NET__PACKET_ERROR, \"Decode %s failed: %s is not a string: %%s\", %s->TypeString());\n"
		"		delete packet;\n"
		"		return false;\n"
		"	}\n"
		"	PyString *%s = (PyString *) %s;\n"
		"	%s = %s->content();\n"
		"",
		v,
			mName, name, v,
		iname, v,
		name, iname
		);
	if(type1 != NULL) {
		//do a check on the type... however thus far we do not care either 
		// way, so dont fail the whole packet just for this
		fprintf(into, 
			"	if(%s->isType1() != %s) {\n"
			"		_log(NET__PACKET_ERROR, \"Decode %s: String type mismatch on %s: expected %%d got %%d. Continuing anyhow.\", %s, %s->isType1());\n"
			"	}\n"
			"",
			iname, atobool( type1 ) ? "true" : "false",
				mName, name, type1, iname
			);
	}
	
	if(is_optional) {
		fprintf(into, "	}\n");
	}
	
	pop();
	return true;
}

bool ClassDecodeGenerator::Process_real(FILE *into, TiXmlElement *field) {
	const char *name = field->Attribute("name");
	if(name == NULL) {
		_log(COMMON__ERROR, "field at line %d is missing the name attribute, skipping.", field->Row());
		return false;
	}
	const char *v = top();

	//this should be done better:
	const char *none_marker = field->Attribute("none_marker");
	bool is_optional = false;
	if(none_marker != NULL) {
		fprintf(into, 
			"	if(%s->IsNone()) {\n"
			"		%s = %s;\n"
			"	} else {\n",
			v,
				name, none_marker);
		is_optional = true;
	}
	
	char iname[16];
	int num = mItemNumber++;
	snprintf(iname, sizeof(iname), "real_%d", num);
	
	fprintf(into, 
		"	if(!%s->IsFloat()) {\n"
		"		_log(NET__PACKET_ERROR, \"Decode %s failed: %s is not a real: %%s\", %s->TypeString());\n"
		"		delete packet;\n"
		"		return false;\n"
		"	}\n"
		"	PyFloat *%s = (PyFloat *) %s;\n"
		"	%s = %s->value;\n"
		"",
		v, mName, name, v, iname, v, name, iname
		);
	
	if(is_optional) {
		fprintf(into, "	}\n");
	}
	
	pop();
	return true;
}






















