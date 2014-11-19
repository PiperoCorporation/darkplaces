#include "xml.h" 
#include <libxml/xmlmemory.h>
#include <libxml/parser.h>

typedef struct
{
	xmlDocPtr  doc;
	xmlNodePtr node;
	xmlAttrPtr attribute;
} qxml_t;

typedef qxml_t* qxml_p;

static qxml_p VM_Xml_Data(prvm_prog_t *prog, int index)
{
	if (index < 0 || index >= PRVM_MAX_OPENFILES)
	{
		Con_Printf("VM_Xml_Data: invalid file handle %i used in %s\n", index, prog->name);
		return NULL;
	}
	if (prog->open_xml_files[index] == NULL)
	{
		Con_Printf("VM_Xml_Data: no such file handle %i (or file has been closed) in %s\n", index, prog->name);
		return NULL;
	}
	return (qxml_p)prog->open_xml_files[index];
}

void XML_Close(prvm_prog_t *prog, int index)
{
	if ( prog->open_xml_files[index] != NULL )
	{
		qxml_p xml = (qxml_p)prog->open_xml_files[index];
		xmlFreeDoc(xml->doc);
		free(xml);
		prog->open_xml_files[index] = NULL;
	}
}

void VM_xml_open(prvm_prog_t *prog)
{
	const char* filename;
	
	char* data;
	size_t datasize;
	qfile_t* filepointer;
	char vabuf[1024];
	
	xmlDocPtr doc;
	int docid;
	qxml_p xml;
	
	VM_SAFEPARMCOUNT(1,VM_xmlopen);
	filename = PRVM_G_STRING(OFS_PARM0);
	
	
	for (docid = 0; docid < PRVM_MAX_OPENFILES; docid++)
		if (prog->open_xml_files[docid] == NULL)
			break;
	if (docid >= PRVM_MAX_OPENFILES)
	{
		PRVM_G_FLOAT(OFS_RETURN) = 0;
		VM_Warning(prog, "VM_xmlopen: %s ran out of file handles (%i)\n", prog->name, PRVM_MAX_OPENFILES);
		return;
	}
	
	filepointer = FS_OpenVirtualFile(va(vabuf, sizeof(vabuf), "data/%s", filename), false);
	if (filepointer == NULL)
		filepointer = FS_OpenVirtualFile(va(vabuf, sizeof(vabuf), "%s", filename), false);
	
	if ( filepointer == NULL )
	{
		PRVM_G_FLOAT(OFS_RETURN) = 0;
		VM_Warning(prog, "VM_xmlopen: Failed to open: %s\n", filename);
		return;
	}
	
	datasize = FS_FileSize(filepointer);
	data = malloc(datasize);
	FS_Read(filepointer,data,datasize);
	FS_Close(filepointer);

	doc = xmlParseMemory(data,datasize);
	free(data);
	
	if (doc == NULL) 
	{
		PRVM_G_FLOAT(OFS_RETURN) = 0;
		VM_Warning(prog, "VM_xmlopen: Failed to load XML: %s\n", filename);
		return;
	}
	
	xml = malloc(sizeof(qxml_t));
	xml->doc = doc;
	xml->node = xmlDocGetRootElement(doc);
	xml->attribute = NULL;
	prog->open_xml_files[docid] = xml;
	
	PRVM_G_FLOAT(OFS_RETURN) = docid+1; // ensure non-zero file id in QuakeC
}

void VM_xml_close(prvm_prog_t *prog)
{
	int fileindex;
	VM_SAFEPARMCOUNT(1,VM_xmlclose);
	fileindex = PRVM_G_FLOAT(OFS_PARM0);
	XML_Close(prog,fileindex-1);
}

// TODO find out if errorreturn is really needed to return 0...
#define VM_XML_CHECK_RETURN(funcname, errorreturn) \
	VM_SAFEPARMCOUNT(1,#funcname); \
	xml = VM_Xml_Data(prog,PRVM_G_FLOAT(OFS_PARM0)-1); \
	if ( !xml ) {\
		errorreturn; \
		return; \
	}\
	if ( !xml->node ) { \
		VM_Warning(prog, #funcname": null node in %s\n",prog->name); \
		errorreturn; \
		return; \
	} \
	Con_Printf(#funcname":\n\tnode: %s\n\ttype: %d\n\tattr: %s\n\tnext: %s\n\tprev: %s\n\tprnt: %s\n\tchld: %s\n",\
		xml->node->name, \
		xml->node->type, \
		xml->attribute ? (char*)xml->attribute->name : "NULL", \
		xml->node->next ? (char*)xml->node->next->name : "NULL", \
		xml->node->prev ? (char*)xml->node->prev->name : "NULL", \
		xml->node->parent ? (char*)xml->node->parent->name : "NULL", \
		xml->node->children ? (char*)xml->node->children->name : "NULL" \
		);
#define VM_XML_CHECK(funcname) VM_XML_CHECK_RETURN(funcname,)

void VM_xml_tree_name(prvm_prog_t *prog)
{
	qxml_p xml;
	const xmlChar* xs;
	char s[VM_STRINGTEMP_LENGTH] = { 0 };
	VM_XML_CHECK(VM_xml_tree_name);
	if ( xml->attribute )
		xs = xml->attribute->name;
	else
		xs = xml->node->name;
	memcpy(s,xs,xmlStrlen(xs));
	PRVM_G_INT(OFS_RETURN) = PRVM_SetTempString(prog, s);
}

void VM_xml_tree_text(prvm_prog_t *prog)
{
	qxml_p xml;
	xmlChar* xs;
	char s[VM_STRINGTEMP_LENGTH] = { 0 };
	VM_XML_CHECK(VM_xml_tree_text);
	
	if ( xml->attribute )
	{
		xs = xmlGetProp(xml->node,xml->attribute->name);
		memcpy(s,xs,xmlStrlen(xs));
		xmlFree(xs);
	}
	else if ( xml->node->type == XML_ELEMENT_NODE )
	{
		xs = xmlNodeListGetString(xml->doc,xml->node->children,1);
		memcpy(s,xs,xmlStrlen(xs));
		xmlFree(xs);
	}
	else
		memcpy(s,xml->node->content,xmlStrlen(xml->node->content));
	PRVM_G_INT(OFS_RETURN) = PRVM_SetTempString(prog, s);
}

void VM_xml_tree_leaf(prvm_prog_t *prog)
{
	qxml_p xml;
	VM_XML_CHECK_RETURN(VM_xml_tree_leaf, PRVM_G_FLOAT(OFS_RETURN) = 0);
	if ( xml->attribute )
		PRVM_G_FLOAT(OFS_RETURN) = 1;
	else
		PRVM_G_FLOAT(OFS_RETURN) = !xml->node->children;
}

void VM_xml_tree_child(prvm_prog_t *prog)
{
	qxml_p xml;
	VM_XML_CHECK(VM_xml_tree_child);
	if ( xml->attribute )
		VM_Warning(prog, "VM_xml_tree_child: trying to get the child of attribute \"%s\"!\n",
				   xml->attribute->name);
	else if ( xml->node->children )
		xml->node = xml->node->children;
	else
		VM_Warning(prog, "VM_xml_tree_child: trying to get the child of leaf element \"%s\"!\n",
				   xml->node->name);	
}

void VM_xml_tree_parent(prvm_prog_t *prog)
{
	qxml_p xml;
	VM_XML_CHECK(VM_xml_tree_parent);
	if ( xml->attribute )
		xml->attribute = NULL;
	else if ( xml->node->parent )
		xml->node = xml->node->parent;
	else
		VM_Warning(prog, "VM_xml_tree_parent: trying to get the parent of root element \"%s\"!\n",
				   xml->node->name);
}

void VM_xml_tree_has_sibling(prvm_prog_t *prog)
{
	qxml_p xml;
	VM_XML_CHECK_RETURN(VM_xml_tree_has_sibling, PRVM_G_FLOAT(OFS_RETURN) = 0);
	if ( xml->attribute )
		PRVM_G_FLOAT(OFS_RETURN) = !!xml->attribute->next;
	else
		PRVM_G_FLOAT(OFS_RETURN) = !!xml->node->next;
}

void VM_xml_tree_next(prvm_prog_t *prog)
{
	qxml_p xml;
	VM_XML_CHECK(VM_xml_tree_next);
	if ( xml->attribute )
	{
		if ( xml->attribute->next )
			xml->attribute = xml->attribute->next;
		else
			VM_Warning(prog,"VM_xml_tree_next: trying to get next sibling of last attribute \"%s\"!\n",
				   xml->attribute->name);
	}
	else if ( xml->node->next )
		xml->node = xml->node->next;
	else
		VM_Warning(prog,"VM_xml_tree_next: trying to get next sibling of last element \"%s\"!\n",
				xml->node->name);
}

void VM_xml_tree_type(prvm_prog_t *prog)
{
	qxml_p xml;
	VM_XML_CHECK_RETURN(VM_xml_tree_type, PRVM_G_FLOAT(OFS_RETURN) = 0);
	if ( xml->attribute )
		PRVM_G_FLOAT(OFS_RETURN) = XML_ATTRIBUTE_NODE;
	else
		PRVM_G_FLOAT(OFS_RETURN) = xml->node->type;
}

void VM_xml_tree_root(prvm_prog_t *prog)
{
	qxml_p xml;
	VM_SAFEPARMCOUNT(1,VM_xml_tree_root);
	xml = VM_Xml_Data(prog,PRVM_G_FLOAT(OFS_PARM0)-1); 
	if ( !xml )
		return;
	xml->node = xmlDocGetRootElement(xml->doc);
}