/*
  Copyright (C) 2006, 2007, 2008, 2009  Anthony Catel <a.catel@weelya.com>

  This file is part of ACE Server.
  ACE is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2 of the License, or
  (at your option) any later version.

  ACE is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with ACE ; if not, write to the Free Software Foundation,
  Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
*/

/* handle_http.c a renommer par core.c */


#include "users.h"
#include "handle_http.h"
#include "raw.h"
#include "utils.h"

const char basic_chars[16] = { 	'a', 'b', 'c', 'd', 'e', 'f', '0', '1',
				'2', '3', '4', '5', '6', '7', '8', '9'
			};


void gen_sessid_new(char *input, acetables *g_ape)
{
	unsigned int i;
	
	do {
		for (i = 0; i < 32; i++) {
			input[i] = basic_chars[rand()%16];
		}
		input[32] = '\0';
	} while(seek_user_id(input, g_ape) != NULL || seek_user_simple(input, g_ape) != NULL); // Colision verification
}



static unsigned int fixpacket(char *pSock, int type)
{
	size_t i, pLen;
	
	pLen = strlen(pSock);
	
	for (i = 0; i < pLen; i++) {
			if (type == 0 && (pSock[i] == '\n' || pSock[i] == '\r')) {
				pSock[i] = '\0';
			
				return 1;
			} else if (type == 1 && pSock[i] == ' ') {
				pSock[i] = '\0';
			
				return 1;				
			}
		
	}
	return 0;
}
int gethost(char *base, char *output) // Catch the host HTTP header
{
	char *pBase;
	int i;
	
	output[0] = '\0';
	
	for (pBase = base; *pBase && strncasecmp(pBase, "Host:", 5) != 0; pBase++);
	
	if (!*pBase || !pBase[6]) {
		return 0;
	}
	
	pBase = &pBase[6];
	
	for (i = 0; pBase[i] && pBase[i] != '\n' && i < (MAX_HOST_LENGTH-1); i++) {
		output[i] = pBase[i];
	}
	
	output[i] = '\0';
	
	return 1;
}

char *getpost(char *input) // Catch Post datas
{
	char *pInput;
	
	for (pInput = input; *pInput && strncmp(pInput, "\r\n\r\n", 4) != 0; pInput++);
	
	if (!*pInput || !pInput[4]) {
		return NULL;
	} else {
		return &pInput[4];
	}
}
int getqueryip(char *base, char *output)
{
	int i, size = strlen(base), step, x = 0;
	
	if (size < 16) {
		return 0;
	}
	
	for (i = 0, step = 0; i < size; i++) {
		if (base[i] == '\n') {
			output[0] = '\0';
			return 0;
		}
		if (step == 1 && (base[i] == '&' || base[i] == ' ') && x < 16) {
			output[x] = '\0';
			return 1;
		} else if (step == 1 && x < 16) {
			output[x] = base[i];
			x++;
		} else if (base[i] == '?') {
			step = 1;
		}
	}
	output[0] = '\0';
	return 0;
	
}

char *getfirstparam(char *input)
{

	char *pInput;
	/*
		Should be replaced by a simple strchr
	*/	
	for (pInput = input; *pInput && *pInput != '&'; pInput++);
	
	if (!*pInput || !pInput[1]) {
		return NULL;
	} else {
		return &pInput[1];
	}	
}

transpipe *init_pipe(void *pipe, int type)
{
	transpipe *npipe = NULL;
	npipe = xmalloc(sizeof(*npipe));
	npipe->pipe = pipe;
	npipe->type = type;
	
	return npipe;
}

subuser *checkrecv(char *pSock, int fdclient, acetables *g_ape, char *ip_client)
{

	unsigned int op;
	subuser *user = NULL;

	clientget *cget = xmalloc(sizeof(*cget));


	if (strlen(pSock) < 3 || getqueryip(pSock, cget->ip_get) == 0) {  // get query IP (from htaccess)
		free(cget);
		shutdown(fdclient, 2);
		return NULL;		
	}

	strncpy(cget->ip_client, ip_client, 16); // get real IP (from socket)
	cget->fdclient = fdclient;
	
	gethost(pSock, cget->host);
	
	
	
	if (strncasecmp(pSock, "GET", 3) == 0) {
		if (fixpacket(pSock, 0) == 0 || (cget->get = getfirstparam(pSock)) == NULL) {
			free(cget);
			
			shutdown(fdclient, 2);
			return NULL;			
		}
	} else if (strncasecmp(pSock, "POST", 4) == 0) {
		if ((cget->get = getpost(pSock)) == NULL) {
			free(cget);
			
			shutdown(fdclient, 2);
			return NULL;			
		}
	} else {
		free(cget);

		shutdown(fdclient, 2);
		return NULL;		
	}
	fixpacket(cget->get, 1);
	
	op = checkraw(cget, &user, g_ape);

	switch (op) {
		case CONNECT_SHUTDOWN:
			shutdown(fdclient, 2);
			
			break;
		case CONNECT_KEEPALIVE:
			break;
	}

	free(cget);
	
	return user;

}
