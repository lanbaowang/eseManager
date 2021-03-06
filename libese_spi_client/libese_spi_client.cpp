#include <jni.h>
#include <fcntl.h>
#include <string.h>
#include <android/log.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <unistd.h>
#include <dirent.h>
#include <fcntl.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <android/log.h>
#include "include/tee_client_api.h"

#include <getopt.h>

#define __DEBUG__
#define LOG_TAG "uca"
#define ALOGI(...)  __android_log_print(ANDROID_LOG_INFO, LOG_TAG,  __VA_ARGS__)


#define ERROR -1
#define INIT_CONTEXT_FAILED (100)
#define OPEN_SESSION_FAILED (101)

#define OPEN_BASIC_CHANNEL    0x0001
#define OPEN_LOGICAL_CHANNEL  0x0002
#define CHANNEL_TRANSMIT      0x0003
#define CLOSE_CHANNEL         0x0004

#define HAL_TRANSMIT          0x0008
#define ESESPI_TEST           0x0009

#define BIG_INT_LENGTH 256

static char hostName[] = "seapitester";
static TEEC_UUID uuid_ta = { 0x01020304, 0x0506, 0x0708, { 0x99, 0x0A, 0x0B,	0x0C, 0x0D, 0x0E, 0x0F, 0x10 } };
static uint8_t getCplc_cmd[]  = {0x80,0xca,0x9f,0x7f,0x00};

struct send_cmd {
    uint32_t cmd_len;
    unsigned char tApdu[200];
};

struct send_cmd_rsp {
    uint32_t rsp_len;
    unsigned char rApdu[200];
};


static int32_t init(TEEC_Context *context, TEEC_Session *session, uint32_t *returnOrigin)
{
	uint32_t returnValue = 0;
	TEEC_Result result;

	do {
		// initialize context
		result = TEEC_InitializeContext(hostName, context);

		if (result != TEEC_SUCCESS) {
			ALOGI("Failed to initialize context");
			returnValue = INIT_CONTEXT_FAILED;
			break;
		}

		// open session
		result = TEEC_OpenSession(context, session, &uuid_ta, TEEC_LOGIN_PUBLIC, NULL, NULL, returnOrigin);

		if (result != TEEC_SUCCESS) {
			ALOGI("Failed to openSession with reason 0x%x", returnOrigin);
			returnValue = OPEN_SESSION_FAILED;
			break;
		}
		returnValue = 0;
	} while (0);
	return returnValue;
}


static void print_packet(const char *pString, const uint8_t *p_data,
        uint16_t len)
{
    uint32_t i;
    char print_buffer[len * 3 + 1];

    memset (print_buffer, 0, sizeof(print_buffer));
    for (i = 0; i < len; i++) {
        snprintf(&print_buffer[i * 3], 4, "%02X ", p_data[i]);
    }
    if( 0 == memcmp(pString,"SEND",0x04))
    {
        printf("SEND:len = %3d > %s \n", len, print_buffer);
    }
    else if( 0 == memcmp(pString,"RECV",0x04))
    {
        printf("RECV:len = %3d < %s \n", len, print_buffer);
    }

    return;
}


int invoke_transmit(TEEC_Session *session,TEEC_Operation *operation)
{
	TEEC_Result result;

	static uint8_t crs_aid[9]={0xA0,0x00,0x00,0x01,0x51,0x43,0x52,0x53,0x00};
	int crs_aid_len=9;

	uint8_t set_status_deactive[17]={0x80,0xF0,0x01,0x00,0x0B,	
								0x4F,0x09,0xA0,0x00,0x00,0x01,0x51,0x41,0x43,0x4C,0x00,0x00};
	uint8_t set_status_active[17]={0x80,0xF0,0x01,0x01,0x0B,	
								0x4F,0x09,0xA0,0x00,0x00,0x01,0x51,0x41,0x43,0x4C,0x00,0x00};
	uint8_t get_status_deactive[21]={0x80,0xF2,0x40,0x00,0x0F, 
								0x4F,0x09,0xA0,0x00,0x00,0x01,0x51,0x41,0x43,0x4C,0x00,0x5C,0x02,0x9F,0x70,0x00};
	int set_status_len=17;
	int get_status_len=21;
	uint8_t resp[100]={0};
	int resp_len;
	
	memcpy(operation->params[0].memref.parent->buffer,crs_aid,crs_aid_len);
	operation->params[0].memref.size=crs_aid_len;
	result = TEEC_InvokeCommand(session, OPEN_LOGICAL_CHANNEL, operation, NULL);
	 if (result != TEEC_SUCCESS){
         return result;
     }
	 else{
	 	resp_len=operation->params[1].memref.size;
		memcpy(resp,operation->params[1].memref.parent->buffer,resp_len);
		printf("InvokeCommand OPEN_LOGICAL_CHANNEL sucess\n");
		print_packet("SEND",crs_aid,crs_aid_len);
		print_packet("RECV",resp,resp_len);
	 }
	
	operation->params[1].memref.size=BIG_INT_LENGTH;        
	memcpy(operation->params[0].memref.parent->buffer,get_status_deactive,get_status_len);
	operation->params[0].memref.size=get_status_len;
	result = TEEC_InvokeCommand(session, CHANNEL_TRANSMIT, operation, NULL);
	if (result != TEEC_SUCCESS){
		return result;
	}
	else{
		resp_len=operation->params[1].memref.size;
		memcpy(resp,operation->params[1].memref.parent->buffer,resp_len);
		printf("InvokeCommand CHANNEL_TRANSMIT sucess\n");
		print_packet("SEND",get_status_deactive,get_status_len);
		print_packet("RECV",resp,resp_len);

	}

	if(resp[6]!=0x00){	
		operation->params[1].memref.size=BIG_INT_LENGTH;
		memcpy(operation->params[0].memref.parent->buffer,set_status_deactive,set_status_len);
		operation->params[0].memref.size=set_status_len;
		result = TEEC_InvokeCommand(session, CHANNEL_TRANSMIT, operation, NULL);
		if (result != TEEC_SUCCESS){
			return result;
		}
		else{
			resp_len=operation->params[1].memref.size;
			memcpy(resp,operation->params[1].memref.parent->buffer,resp_len);
			printf("InvokeCommand CHANNEL_TRANSMIT sucess\n");
			print_packet("SEND",set_status_deactive,set_status_len);
			print_packet("RECV",resp,resp_len);
		}
	}
	else{
		operation->params[1].memref.size=BIG_INT_LENGTH;
		memcpy(operation->params[0].memref.parent->buffer,set_status_active,set_status_len);
		operation->params[0].memref.size=set_status_len;
		result = TEEC_InvokeCommand(session, CHANNEL_TRANSMIT, operation, NULL);
		if (result != TEEC_SUCCESS){
			return result;
		}
		else{
			resp_len=operation->params[1].memref.size;
			memcpy(resp,operation->params[1].memref.parent->buffer,resp_len);
			printf("InvokeCommand CHANNEL_TRANSMIT sucess\n");
			print_packet("SEND",set_status_active,set_status_len);
			print_packet("RECV",resp,resp_len);
		}

	}

	operation->params[1].memref.size=BIG_INT_LENGTH;
	memcpy(operation->params[0].memref.parent->buffer,get_status_deactive,get_status_len);
	operation->params[0].memref.size=get_status_len;
	result = TEEC_InvokeCommand(session, CHANNEL_TRANSMIT, operation, NULL);
	if (result != TEEC_SUCCESS){
		return result;
	}
	else{
		resp_len=operation->params[1].memref.size;
		memcpy(resp,operation->params[1].memref.parent->buffer,resp_len);
		printf("InvokeCommand CHANNEL_TRANSMIT sucess\n");
		print_packet("SEND",get_status_deactive,get_status_len);
		print_packet("RECV",resp,resp_len);

	}

	operation->params[1].memref.size=BIG_INT_LENGTH;
	result = TEEC_InvokeCommand(session, CLOSE_CHANNEL, operation, NULL);
	if (result != TEEC_SUCCESS){
		return result;
	}
	else{
		printf("InvokeCommand CLOSE_CHANNEL sucess\n");
		return result;
	}


}

static uint8_t char_to_byte(char char_in)
{
	if((char_in>='0')&&(char_in<='9'))
		return (uint8_t)(char_in-'0');
	else if((char_in>='a')&&(char_in<='f'))
		return ((uint8_t)(char_in-'a')+10);
	else if((char_in>='A')&&(char_in<='F'))
		return ((uint8_t)(char_in-'A')+10);
	else
		return ERROR;
}


static void hexchar_to_byte(char *hexstring, uint8_t *byte, int *byte_len)
{
	int i,j;
	i=0;
	j=0;
	while(hexstring[i]!='\0')
		{
			byte[j]=(char_to_byte(hexstring[i])<<4)|char_to_byte(hexstring[i+1]);
			i+=2;
			j+=1;
		}
	*byte_len=j;
}

static int32_t invoke_hal_transmit(TEEC_Session *session,TEEC_Operation *operation,uint8_t *hal_cmd,int hal_cmd_len)
{
	uint8_t resp[100]={0};
	int resp_len;
	TEEC_Result ret;

	operation->params[1].memref.size=BIG_INT_LENGTH;
	
	memcpy(operation->params[0].memref.parent->buffer,hal_cmd,hal_cmd_len);
	operation->params[0].memref.size=hal_cmd_len;
	
	ret =  TEEC_InvokeCommand(session, HAL_TRANSMIT, operation, NULL);
	if (ret != TEEC_SUCCESS){
		printf("invoke_hal_transmit fail\n");
	}
 	else{
		resp_len=operation->params[1].memref.size;
		memcpy(resp,operation->params[1].memref.parent->buffer,resp_len);
		printf("InvokeCommand HAL_TRANSMIT sucess\n");
		print_packet("SEND",hal_cmd,hal_cmd_len);
		print_packet("RECV",resp,resp_len);
 	}

	return ret;

}

int32_t ca_sendEseCommand(void* cmd, uint32_t cmdLen, void* rsp, uint32_t rspLen)
{

	TEEC_Context context = {0};
	TEEC_Session session = {0};
	TEEC_Operation operation = {0};
	TEEC_Result result;
	uint32_t returnOrigin = 0;
	int32_t ret = 0;
	TEEC_SharedMemory inputSm = {0};
	TEEC_SharedMemory outputSm = {0};

	if ((!cmd) || (!rsp) || (cmdLen == 0) || (rspLen == 0)) {
		ALOGI("NULL params");
		return -1;
	}
	struct send_cmd *cmd_ptr = (send_cmd *) cmd;
	struct send_cmd_rsp *rsp_ptr = (send_cmd_rsp *) rsp;
	
	//TEEC_InitializeContext and TEEC_OpenSession
	ret = init(&context, &session, &returnOrigin);
	switch (ret) {
		case INIT_CONTEXT_FAILED:
			result = INIT_CONTEXT_FAILED;
			goto cleanup1;
		break;
		case OPEN_SESSION_FAILED:
			result = OPEN_SESSION_FAILED;
			goto cleanup2;
		break;
		default:
			ALOGI("Init TA success!");
		break;
	}

	memset(&inputSm, 0x00, sizeof(inputSm));
	inputSm.size = cmd_ptr->cmd_len;
	inputSm.flags = TEEC_MEM_INPUT;
	result = TEEC_AllocateSharedMemory(&context, &inputSm);
	if (result != TEEC_SUCCESS) {
		ALOGI("TEEC_AllocateSharedMemory input failed!");
		goto cleanup4;
	}
	memcpy(inputSm.buffer, cmd_ptr->tApdu, cmd_ptr->cmd_len);

	outputSm.size = rspLen;
	outputSm.flags = TEEC_MEM_OUTPUT;
	result = TEEC_AllocateSharedMemory(&context, &outputSm);
	if (result != TEEC_SUCCESS) {
		ALOGI("TEEC_AllocateSharedMemory output failed!");
		goto cleanup5;
	}
	memset(outputSm.buffer, 0x00, rspLen);

	
	operation.paramTypes = TEEC_PARAM_TYPES(TEEC_MEMREF_PARTIAL_INPUT, TEEC_MEMREF_PARTIAL_OUTPUT,
								TEEC_NONE, TEEC_NONE);
	operation.started = 1;
	operation.params[0].memref.parent = &inputSm;
	operation.params[0].memref.offset = 0;
	operation.params[0].memref.size = inputSm.size;

	operation.params[1].memref.parent = &outputSm;
    operation.params[1].memref.offset = 0;
    operation.params[1].memref.size = outputSm.size;;
	
	result =  TEEC_InvokeCommand(&session, HAL_TRANSMIT, &operation, NULL);
	if (result != TEEC_SUCCESS) {
		ALOGI("TEEC_InvokeCommand failed!");
		goto cleanup5;
	} else {
		rsp_ptr->rsp_len = operation.params[1].memref.size;
		memcpy(rsp_ptr->rApdu, operation.params[1].memref.parent->buffer, rsp_ptr->rsp_len);
	}
cleanup5:
	TEEC_ReleaseSharedMemory(&outputSm);
cleanup4:
	TEEC_ReleaseSharedMemory(&inputSm);
cleanup3:
    TEEC_CloseSession(&session);
cleanup2:
    TEEC_FinalizeContext(&context);
cleanup1:
  	return (0 - result);
}

int32_t ca_getEseCplc (uint8_t* cplc_buf, uint32_t* cplc_len)
{

	TEEC_Context context = {0};
	TEEC_Session session = {0};
	TEEC_Operation operation = {0};
	TEEC_Result result;
	uint32_t returnOrigin = 0;
	int32_t ret = 0;
	TEEC_SharedMemory inputSm = {0};
	TEEC_SharedMemory outputSm = {0};

	if (cplc_buf == NULL || *cplc_len == 0) {
		return -EINVAL;
	}

	//TEEC_InitializeContext and TEEC_OpenSession
	ret = init(&context, &session, &returnOrigin);
	switch (ret) {
		case INIT_CONTEXT_FAILED:
			result = INIT_CONTEXT_FAILED;
			goto cleanup1;
		break;
		case OPEN_SESSION_FAILED:
			result = OPEN_SESSION_FAILED;
			goto cleanup2;
		break;
		default:
			ALOGI("Init TA success!");
		break;
	}

	memset(&inputSm, 0x00, sizeof(inputSm));
	inputSm.size = sizeof(getCplc_cmd);
	inputSm.flags = TEEC_MEM_INPUT;
	result = TEEC_AllocateSharedMemory(&context, &inputSm);
	if (result != TEEC_SUCCESS) {
		ALOGI("TEEC_AllocateSharedMemory input failed!");
		goto cleanup4;
	}
	memcpy(inputSm.buffer, getCplc_cmd, sizeof(getCplc_cmd));

	outputSm.size = *cplc_len;
	outputSm.flags = TEEC_MEM_OUTPUT;
	result = TEEC_AllocateSharedMemory(&context, &outputSm);
	if (result != TEEC_SUCCESS) {
		ALOGI("TEEC_AllocateSharedMemory output failed!");
		goto cleanup5;
	}
	memset(outputSm.buffer, 0x00, *cplc_len);

	
	operation.paramTypes = TEEC_PARAM_TYPES(TEEC_MEMREF_PARTIAL_INPUT, TEEC_MEMREF_PARTIAL_OUTPUT,
								TEEC_NONE, TEEC_NONE);
	operation.started = 1;
	operation.params[0].memref.parent = &inputSm;
	operation.params[0].memref.offset = 0;
	operation.params[0].memref.size = inputSm.size;

	operation.params[1].memref.parent = &outputSm;
    operation.params[1].memref.offset = 0;
    operation.params[1].memref.size = outputSm.size;;
	
	result =  TEEC_InvokeCommand(&session, HAL_TRANSMIT, &operation, NULL);
	if (result != TEEC_SUCCESS) {
		ALOGI("TEEC_InvokeCommand failed!");
		goto cleanup5;
	} else {
		*cplc_len = operation.params[1].memref.size;
		memcpy(cplc_buf, operation.params[1].memref.parent->buffer, *cplc_len);
	}
cleanup5:
	TEEC_ReleaseSharedMemory(&outputSm);
cleanup4:
	TEEC_ReleaseSharedMemory(&inputSm);
cleanup3:
    TEEC_CloseSession(&session);
cleanup2:
    TEEC_FinalizeContext(&context);
cleanup1:
  	return (0 - result);
}
int32_t teeclientdemo(int argc, char *argv[]) 
{
        TEEC_Context context;
        TEEC_Session session;
        TEEC_Operation operation;
        TEEC_Result result;
        uint32_t returnOrigin;
        int32_t ret;
        int32_t returnValue;
        TEEC_SharedMemory bigInt1;
        TEEC_SharedMemory bigOut;
     
		int opt;
		char *arg_p;
		int i,j;
		uint8_t hal_cmd[256];
		int hal_cmd_len;


        ret = init(&context, &session, &returnOrigin);
        if (ret == INIT_CONTEXT_FAILED)
        {
                goto cleanup1;
        }
        else if (ret == OPEN_SESSION_FAILED)
        {
                goto cleanup2;
        }

        bigInt1.size = BIG_INT_LENGTH;
        bigInt1.flags = TEEC_MEM_INPUT;
        result = TEEC_AllocateSharedMemory(&context, &bigInt1);
        memset(bigInt1.buffer, 0xff, bigInt1.size);
        memset(bigInt1.buffer, 0x0, 1);


        bigOut.size = BIG_INT_LENGTH;
        bigOut.flags = TEEC_MEM_OUTPUT;
        result = TEEC_AllocateSharedMemory(&context, &bigOut);
        memset(bigOut.buffer, 0, bigOut.size);

        // set operation parameters
        memset(&operation, 0x00, sizeof(operation));

        operation.paramTypes = TEEC_PARAM_TYPES(TEEC_MEMREF_PARTIAL_INPUT, TEEC_MEMREF_PARTIAL_OUTPUT,
								TEEC_NONE, TEEC_NONE);
        operation.started = 1;

        operation.params[0].memref.parent = &bigInt1;
        operation.params[0].memref.offset = 0;
        operation.params[0].memref.size = BIG_INT_LENGTH;


        operation.params[1].memref.parent = &bigOut;
        operation.params[1].memref.offset = 0;
        operation.params[1].memref.size = BIG_INT_LENGTH;

		printf("Usage:./teeclientdemo -c -t -i data\n");

		while((opt = getopt(argc, argv,"cti:")) != -1)
		{
			switch (opt)
			{
			case 'c':
				invoke_transmit(&session,&operation);
				break;
			case 't':
				ret = TEEC_InvokeCommand(&session, ESESPI_TEST, &operation, NULL);
				if (ret == 0){
					printf("ESESPI_TEST sucess\n");
				}
				else{
					printf("ESESPI_TEST fail, ret=%d\n",ret);
				}
				break;
			case 'i':
				hexchar_to_byte(optarg,hal_cmd,&hal_cmd_len);
	//			print_packet("SEND",hal_cmd,hal_cmd_len);
				invoke_hal_transmit(&session,&operation,hal_cmd,hal_cmd_len);
				
				break;
			}
		}


cleanup4:
		TEEC_ReleaseSharedMemory(&bigInt1);
		TEEC_ReleaseSharedMemory(&bigOut);
cleanup3:
        TEEC_CloseSession(&session);
cleanup2:
        TEEC_FinalizeContext(&context);
cleanup1:
        return 0;
}
