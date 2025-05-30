/*
 * httpd_cgi.h
 *
 *  Created on: 26 jul. 2012
 *      Author: dig
 */

#ifndef HTTPD_CGI_H_
#define HTTPD_CGI_H_

#include "../../http/include/httpd.h"
#define CGIRETURNFILE "/CGIreturn.txt"

extern bool sendBackOK;

extern CGIresponseFileHandler_t readResponseFile;

int actionRespScript(char *pBuffer, int count);
int freadCGI( char *buffer, int count);
void CGI_init( void );

extern const tCGI *g_pCGIs;
//extern int g_iNumCGIs;
typedef enum { FLT, STR, INT , DESCR , CALVAL} varType_t;

typedef struct {
	const char *name;
	void *pValue;
	varType_t type;
	int nrValues;
} CGIdesc_t;

extern const CGIdesc_t writeVarDescriptors[];
void parseCGIWriteData(char * buf, int received);
int readActionScript(char *pcParam, const CGIdesc_t *CGIdescTable, int size );
const char* startCGIscript(int iIndex, char *pcParam);
const char* readCGIvalues(int iIndex, char *pcParam);
int readVarScript(char *pBuffer, int count);
int actionRespScript(char *pBuffer, int count);

#endif /* HTTPD_CGI_H_ */
