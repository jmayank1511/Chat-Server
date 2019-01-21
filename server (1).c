//todo: remove all shared mem ipcs
//todo add comments
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <sys/shm.h>
#include <signal.h>
#define PORT 8887
#define ROWS_FOR_MSG_TABLE 50
#define ROWS_FOR_GRP_TABLE 5
#define P(s) semop(s, &pop, 1)  
#define V(s) semop(s, &vop, 1)
#define MAX_CLIENT_LIMIT 5
#define MAX_GROUP_LIMIT 5
#define BROADCAST 1
#define INDIVIDUAL 2
#define GROUP 3
#define EMPTY 0
#define FULL 1
#define NOT_ACCEPTED 0
#define ACCEPTED 1
struct message_table
{
	int status; // EMPTY or FULL
	long to,from;
	int type; // type = BROADCAST or INDIVIDUAL or GROUP
	char msg[1000];
	int grpID;
	long clients[MAX_CLIENT_LIMIT];
};
struct group_table
{
	int status; // EMPTY or FULL
	long members[MAX_CLIENT_LIMIT];
	long requests[MAX_CLIENT_LIMIT];
	int grpID;
	long admin;
	int memberCount;
};
struct request_table
{
	int status;
	int grp;
	long reqFor,admin;
};
struct clientData
{
	long id;
	int grpCount;
	int adminCount;
	int groups[MAX_GROUP_LIMIT];
	int adminOf[MAX_GROUP_LIMIT];
};
typedef struct message_table msg_table;
typedef struct group_table grp_table;
typedef struct clientData client_info;
typedef struct request_table req_table;
msg_table *msgTable;  // this str stores all the messages
grp_table *grpTable;  // this str stores all the group info
req_table *reqTable;
client_info *clientInfo;
int semid1,semid2,shmid,msgTableid,grpTableid,activeClientsid,*noOfClients,groupno,*groupId,reqId;
void error(char *msg)
{
	perror(msg);
	exit(1);
}
void handle_sigint(int sig) 
{ 
	semctl(semid1, 0, IPC_RMID, 0);
	semctl(semid2, 0, IPC_RMID, 0);
	shmctl(shmid, IPC_RMID, 0);
	shmctl(msgTableid, IPC_RMID, 0);
	shmctl(grpTableid, IPC_RMID, 0);
	shmctl(activeClientsid, IPC_RMID, 0);
	shmctl(groupno, IPC_RMID, 0);
	shmctl(reqId, IPC_RMID, 0); 
	exit(0);
} 
void clearAllMemoryAreas()
{
	int i;
	for(i=0;i<ROWS_FOR_MSG_TABLE;i++)
	{
		msgTable[i].status=EMPTY;
	}
	for(i=0;i<ROWS_FOR_GRP_TABLE;i++)
	{
		grpTable[i].status=EMPTY;
	}
	for(i=0;i<MAX_CLIENT_LIMIT;i++)
	{
		clientInfo[i].id=0;
	}
	for(i=0;i<20;i++)
	{
		reqTable[i].status=EMPTY;
	}
}
int performReading(int len , char input[len],long myId)
{
	int i;
	for(i=0;i<ROWS_FOR_MSG_TABLE;i++)
	{
		if(msgTable[i].status==FULL)
		{
			if(msgTable[i].to==myId)
			{
				char temp[100];
				bzero(temp,30);
				bzero(input,1024);
				if(msgTable[i].type==BROADCAST)
				{
					sprintf(temp,"Broadcast Message by: %ld\n",msgTable[i].from );
					strcat(input,temp);
					strcat(input, "Message: ");
					strcat(input,msgTable[i].msg);
					msgTable[i].status=EMPTY;
				}
				else if(msgTable[i].type==GROUP)
				{
					sprintf(temp,"Message by: %ld in group: %d\n",msgTable[i].from ,msgTable[i].grpID);
					strcat(input,temp);
					strcat(input, "Message: ");
					strcat(input,msgTable[i].msg);
					msgTable[i].status=EMPTY;
				}
				else if(msgTable[i].type==INDIVIDUAL)
				{
					sprintf(temp,"Message by: %ld\n",msgTable[i].from);
					strcat(input,temp);
					strcat(input, "Message: ");
					strcat(input,msgTable[i].msg);
					msgTable[i].status=EMPTY;
				}
			return 1;	
			}
		}
	}
	return 0;
}
int checkIfExist(long id)
{
	int i;
	for(i=0;i<MAX_CLIENT_LIMIT;i++)
	{
		if(clientInfo[i].id==id)
			return 1;
	}
	return 0;
}
int checkIfExistGrpid(int grp,long client_id) // checks if group exists and user is member of it
{
	int i;
	for(i=0;i<ROWS_FOR_GRP_TABLE;i++)
	{
		if(grpTable[i].status==FULL)
		{
			if(grpTable[i].grpID==grp)
			{	
				int j; // check if i am member of this group
				for(j=0;j<MAX_CLIENT_LIMIT-1;j++)
				{
					if(grpTable[i].members[j]==client_id)					
						return i;											
				}
			}
		}
	}
	return -1;
}
int checkIfExistGrpidV2(int grp,long client_id) // checks if group exist
{
	int i;
	for(i=0;i<ROWS_FOR_GRP_TABLE;i++)
	{
		if(grpTable[i].status==FULL)
		{
			if(grpTable[i].grpID==grp)
			{	
				int j; // check if i am member of this group
				for(j=0;j<MAX_CLIENT_LIMIT-1;j++)
				{
					if(grpTable[i].requests[j]==client_id)					
						return i;											
				}
				return -2;
			}
		}
	}
	return -1;
}
void editMemberInfo(long id,int group__ID,int adm)
{
	int i;
	for(i=0;i<MAX_CLIENT_LIMIT;i++)
	{
		if(clientInfo[i].id==id)
		{
			clientInfo[i].groups[clientInfo[i].grpCount]=group__ID;
			clientInfo[i].grpCount++;
			if(adm)
			{
				clientInfo[i].adminOf[clientInfo[i].adminCount]=group__ID;
				clientInfo[i].adminCount++;
			}

			break;
		}
	}
}
void removeMemberFromGroup(long member,int grpid,long myId)
{
	int i,j;
	for(j=0;j<MAX_CLIENT_LIMIT;j++)
	{
		if(clientInfo[j].id==member)
		{
			break;
		}
	}
	for(i=0;i<MAX_GROUP_LIMIT;i++ )
	{
		if(clientInfo[j].groups[i]==grpid)
		{
			clientInfo[j].groups[i]=0;
			break;
		}
	}
}
void fActiveClients(int len,char data[len],int fd,long myId)
{
	char command[50],dummy[250];
	int flag=0;
	int ret = sscanf(data,"%s %[^\n]",command,dummy);
	printf("Client %ld requested for : %s\n",myId,data);
	if(ret==1)		
	{
		if(!strcmp("/active",command)) // if command is syntically correct
		{
			char data1[1024]; // char to store the output
			bzero(data1,1024);
			strcat(data1,"Active Clients: ");
			int i;
			int flag=0;
			for(i =0;i<MAX_CLIENT_LIMIT;i++)
			{
				if(clientInfo[i].id!=0)
				{
					flag =1;
					char temp[100];
					snprintf (temp, sizeof(temp), "%ld",clientInfo[i].id);
					strcat(data1,temp);
					strcat(data1," ");
				}
			}
			data[strlen(data1)]='\0';
			if(flag==1)
			{ 
				int n = send(fd,data1,strlen(data1),0);  
				if (n <= 0) 
					printf("ERROR writing to socket");
				printf("Output:client:%ld: %s\n",myId,data1);
			}
			bzero(data1,strlen(data1));
			return;
		}
	}
	int n = send(fd,"Incorrect Syntax.\n",19,0);  
	if (n <= 0) 
		error("ERROR writing to socket");
	printf("Output:client:%ld: Incorrect Syntax\n",myId);
}
void fSend(int len,char data[len],int fd,long myId)
{
	char command[50],message[100];
	long to;
	int ret = sscanf(data,"%s %ld %[^\n]",command,&to,message);
	if(ret==3)
	{
		if(checkIfExist(to))//check if client id 'to' exist or not
		{ // write msg to msg table
			int i;
			for(i=0;i<ROWS_FOR_MSG_TABLE;i++)
			{
				if(msgTable[i].status==EMPTY) // we have a place in msg table to put new messages
				{
					msgTable[i].status=FULL;	
					msgTable[i].to = to;
					msgTable[i].from = myId;
					msgTable[i].type = INDIVIDUAL;
					strcpy(msgTable[i].msg , message);
					break;
				}
			}	
			int n = send(fd,"Message sent successfully..\n",27,0);  
			if (n <= 0) 
				error("ERROR writing to socket");
			printf("Output: Message :'%s' by client: '%ld' to client '%ld' added to table\n",message,myId,to);
			return;
		}
		else
		{
			int n = send(fd,"Invalid Client ID\n",19,0);  
			if (n <= 0) 
				error("ERROR writing to socket");
			printf("Output: client:%ld: Invalid Client Id\n",myId);
			return;
		}	
	}
	int n = send(fd,"Incorrect Syntax.\n",19,0);  
	if (n <= 0) 
		error("ERROR writing to socket");
	printf("Output: client: %ld: Incorrect Syntax\n",myId);
}
void fBroadcast(int len,char buffer[len],int fd,long myId)
{
	if(*noOfClients==1)
	{
		int n = send(fd,"No client to read your broadcast\n",34,0);  
		if (n <= 0) 
			error("ERROR writing to socket");
		printf("Output: client:%ld: No client to read your broadcast\n",myId);
		return;
	}
	char command[50],message[100];
	int ret = sscanf(buffer,"%s %[^\n]",command,message);
	if(ret==2)
	{
		int i,j;
		for(j=0;j<MAX_CLIENT_LIMIT;j++)   //BROADCAST = Multiple Unicast
		{
			int cid = clientInfo[j].id;
			if(cid!=0&&cid!=myId) 
			{
				for(i=0;i<ROWS_FOR_MSG_TABLE;i++)
				{
					if(msgTable[i].status==EMPTY) // we have a place in msg table to put new messages
					{
						msgTable[i].status=FULL;
						msgTable[i].from = myId;
						msgTable[i].to = cid;
						msgTable[i].type = BROADCAST;
						strcpy(msgTable[i].msg , message);
						break;
					}
				}
			}	
	    }	
		int n = send(fd,"Message broadcast successful..\n",30,0);  
		if (n <= 0) 
			error("ERROR writing to socket");
		printf("Output: Message :'%s' by client: '%ld' added to table for broadcast.\n",message,myId);
		return;
	}
	else
	{
		int n = send(fd,"Incorrect Syntax.\n",19,0);  
		if (n <= 0) 
			error("ERROR writing to socket");
		printf("Output: client : %ld : Incorrect Syntax\n",myId);
	}
}
void FmakeGroup(int len,char buffer[len],int fd,long myId)
{
	//code
	char tokns[500];
	bzero(tokns,500);
	strcpy(tokns,buffer);
	char* token = strtok(tokns, " "); 
	int cnt=0;
    while (token != NULL) 
	{ 
		cnt++;
        token = strtok(NULL, " "); 
    }

	char command[50],temp[50];
	long c[MAX_CLIENT_LIMIT-1]; // atmost 4 clients and the admin itself
	int ret = sscanf(buffer,"%s %ld %ld %ld %ld %[^\n]",command,&c[0],&c[1],&c[2],&c[3],temp);
	if(!((ret==cnt)||(ret+1==cnt)))
	{
		int n = send(fd,"Invalid Syntax\n",16,0);
		printf("Output: Client:%ld: Invalid Syntax\n\n",myId);  
		if (n <= 0) 
			error("ERROR writing to socket");
		return;
	}
	if(ret>1&&ret<6)
	{
		int i,j;
		int myGroupId =  ++(*groupId); //todo put it in P and V
		for(i=0;i<MAX_GROUP_LIMIT;i++)
		{
			if(grpTable[i].status==EMPTY)
			{
				grpTable[i].status= FULL;
				grpTable[i].grpID=myGroupId;
				grpTable[i].admin=myId;
				grpTable[i].members[0]=myId;
				break;
			}
		}
		int idx=1;
		char membersData[100];
		bzero(membersData,100);
		if(i!=MAX_GROUP_LIMIT)
		{		
			char temp[30];
			bzero(temp,30);
			sprintf(temp, "%ld", myId); 
			strcat(membersData, "(");
			strcat(membersData,temp);
			strcat(membersData,")Admin, ");	
			for(j=1;j<ret;j++)
			{
				if(checkIfExist(c[j-1])&&c[j-1]!=myId)
				{
					//check if client is already a member
					int chk,sts=0;;
					for(chk=0;chk<idx;chk++)
					{
						if(grpTable[i].members[chk]==c[j-1])
						{
							sts=1;
							break;
						}
					}
					if(!sts)
					{
						grpTable[i].members[idx]=c[j-1];
						sprintf(temp, "%ld", c[j-1]);
						editMemberInfo(c[j-1],myGroupId,0);
						strcat(membersData,temp);
						strcat(membersData," ");
						idx++;
					}

				}		
			}
			if(idx!=1)
			{
				editMemberInfo(myId,myGroupId,1);
				grpTable[i].memberCount=idx;
				char reply[100];
				sprintf(reply,"Group Created: groupid: %d\nNo of Members:%d\nMembers:%s\n",myGroupId,idx,membersData);
				int n = send(fd,reply,strlen(reply),0);  
				if (n <= 0) 
					error("ERROR writing to socket");
				printf("Output: client : %ld\n%s",myId,reply);
				for(j=1;j<ret;j++)
				{
					if(checkIfExist(c[j-1])&&c[j-1]!=myId)
					{	//add group creation msg in message table
						int idx1;
						for(idx1 = 0;idx1<ROWS_FOR_MSG_TABLE;idx1++)
						{				
							if(msgTable[i].status==EMPTY) // we have a space in msg table to put new messages
							{
								msgTable[idx1].status=FULL;
								msgTable[idx1].from = myId;
								msgTable[idx1].to = c[j-1];
								msgTable[idx1].type = GROUP;
								msgTable[idx1].grpID=myGroupId;
								strcpy(msgTable[idx1].msg , reply);
								break;
							}
						}
					}
					
				} //todo : check what happens if more than 5 groups are created
			}
			else
			{
				grpTable[i].status= EMPTY;
				int n = send(fd,"Group with single member can't be created\n",43,0);  
				if (n <= 0) 
					error("ERROR writing to socket");
				printf("Output: Client :%ld : Group with single member can't be created\n",myId);
			}
		}
		else
		{
			int n = send(fd,"No room to add group\n",22,0);
			printf("Output: Client:%ld: No room to add group\n",myId);  
			if (n <= 0) 
				error("ERROR writing to socket");
		}
		return;
	}
	int n = send(fd,"Invalid Syntax\n",16,0);
	printf("Output: Client:%ld: Invalid Syntax\n",myId);  
	if (n <= 0) 
		error("ERROR writing to socket");
}
void fsendGroup(int len,char buffer[len],int fd,long myId)
{
	char command[50],message[100];
	int grp;
	int ret = sscanf(buffer,"%s %d %[^\n]",command,&grp,message);
	if(ret==3)
	{
		int groupNumber = checkIfExistGrpid(grp,myId); // checks if i am member of that group or not
		if(groupNumber!=-1) 
		{ 
			int i,j;
			for( j=0;j<MAX_GROUP_LIMIT;j++)  
			{
				int cid = grpTable[groupNumber].members[j];
				if(cid!=0&&cid!=myId)
				{
					for(i=0;i<ROWS_FOR_MSG_TABLE;i++)
					{
						if(msgTable[i].status==EMPTY) // we have a place in msg table to put new messages
						{
							msgTable[i].status=FULL;
							msgTable[i].from = myId;
							msgTable[i].to = cid;
							msgTable[i].type = GROUP;
							msgTable[i].grpID = grp;
							strcpy(msgTable[i].msg , message);
							break;
						}
					}
				}	
			}	
			int n = send(fd,"Message Sent to Group\n",23,0);  
			if (n <= 0) 
				error("ERROR writing to socket");
			printf("Output: Client: %ld Message sent to Group \n",myId);
			
		}
		else
		{
			int n = send(fd,"Invalid Group no, or you are not the member of the group\n",58,0);  
			if (n <= 0) 
				error("ERROR writing to socket");
			printf("Output: client: %ld : Invalid Group no, or you are not the member of the group\n",myId);
		}
	}
	else
	{
		int n = send(fd,"Incorrect Syntax.\n",19,0);  
		if (n <= 0) 
			error("ERROR writing to socket");
		printf("Output: client: %ld: Incorrect Syntax\n",myId);
	}
}
void fActiveGroups(int len,char buffer[len],int fd,long myId)
{
	char command[50],dummy[100];
	int ret = sscanf(buffer,"%s %[^\n]",command,dummy);
	if(ret==1)
	{
		int i;
		for(i=0;i<MAX_CLIENT_LIMIT;i++)
		{
			if(clientInfo[i].id==myId)
			{
				char temp[200],temp1[100];
				bzero(temp,200);
				bzero(temp1,100);
				strcat(temp,"Groups : ");
				strcat(temp1,"\nAdmin of: ");
				int j,count1=0,count2=0;
				for(j=0;j<MAX_GROUP_LIMIT;j++)
				{
					printf("client: %ld: grp %d val: %d\n",myId,j,clientInfo[i].groups[j]);
					if(clientInfo[i].groups[j]!=0)
					{
						count1++;
						char tmp[100];
						bzero(tmp,100);
						sprintf(tmp,"%d ",clientInfo[i].groups[j]);
						strcat(temp,tmp);
					}
					if(clientInfo[i].adminOf[j]!=0)
					{
						count2++;
						char tmp[100];
						bzero(tmp,100);
						sprintf(tmp,"%d ",clientInfo[i].adminOf[j]);
						strcat(temp1,tmp);
					}
				}
				if(count1>0) // client is member of some group
				{
					if(count2>0) // client is also admin of some group(s)
						strcat(temp,temp1);
					int n = send(fd,temp,strlen(temp),0);  
					if (n <= 0) 
						error("ERROR writing to socket");
					printf("Output: Client: %ld %s\n",myId,temp);
				}
				else
				{	//client is not member of any group
					int n = send(fd,"client is not member of any group",33,0);  
					if (n <= 0) 
						error("ERROR writing to socket");
					printf("Output: Client: %ld :client is not member of any group\n",myId);
				}
				break;
			}
		}	
	}
	else
	{
		int n = send(fd,"Invalid Syntax\n",16,0);  
		if (n <= 0) 
			error("ERROR writing to socket");
		printf("Output: Client: %ld :Invalid Syntax\n",myId);
	}
}
void fmakeGroupReq(int len,char buffer[len],int fd,long myId)
{
	char command[50],temp[50];
	long c[MAX_CLIENT_LIMIT-1]; // atmost 4 clients and the admin itself
	int ret = sscanf(buffer,"%s %ld %ld %ld %ld %[^\n]",command,&c[0],&c[1],&c[2],&c[3],temp);
	if(ret>1&&ret<6)
	{
		int i,j;
		int myGroupId =  ++(*groupId); //todo put it in P and V
		for(i=0;i<MAX_GROUP_LIMIT;i++)
		{
			if(grpTable[i].status==EMPTY)
			{
				grpTable[i].status= FULL;
				grpTable[i].grpID=myGroupId;
				grpTable[i].admin=myId;
				grpTable[i].members[0]=myId;
				break;
			}
		}
		int idx=1;
		char membersData[100];
		bzero(membersData,100);
		if(i!=MAX_GROUP_LIMIT)
		{	
			char temp[30];
			bzero(temp,30);
			sprintf(temp, "%ld", myId); 
			strcat(membersData, "(");
			strcat(membersData,temp);
			strcat(membersData,")Admin, Members Requested: ");	
			for(j=1;j<ret;j++)
			{
				if(checkIfExist(c[j-1])&&c[j-1]!=myId)
				{
					grpTable[i].requests[idx]=c[j-1];
					sprintf(temp, "%ld", c[j-1]);
					strcat(membersData,temp);
					strcat(membersData," ");
					idx++;
				}		
			}
			if(idx==1)
			{ // no client requested grp will not be formed
				grpTable[i].status= EMPTY;
				grpTable[i].grpID=0;
				grpTable[i].admin=0;
				grpTable[i].members[0]=0;
				int n = send(fd,"No Members Requested\n",22,0);  
				if (n <= 0) 
				error("ERROR writing to socket");
				printf("Client %ld: Output: No Members Requested\n",myId);
				return;
			}
			editMemberInfo(myId,myGroupId,1);
			grpTable[i].memberCount=1;
			char reply[100];
			sprintf(reply,"Group Created: groupid: %d\nNo of Members:1\n%s\n",myGroupId,membersData);
			int n = send(fd,reply,strlen(reply),0);  
			if (n <= 0) 
				error("ERROR writing to socket");
			printf("Output: client : %ld\n%s",myId,reply);
			char t[100];
			sprintf(t,"\nType '/joingroup %d' to join the group",myGroupId);
			strcat(reply,t);
			for(j=1;j<ret;j++)
			{
				if(checkIfExist(c[j-1])&&c[j-1]!=myId)
				{//add group creation msg in message table
					int idx1;
					for(idx1 = 0;idx1<ROWS_FOR_MSG_TABLE;idx1++)
					{				
						if(msgTable[i].status==EMPTY) // we have a space in msg table to put new messages
						{
							msgTable[idx1].status=FULL;
							msgTable[idx1].from = myId;
							msgTable[idx1].to = c[j-1];
							msgTable[idx1].type = GROUP;
							msgTable[idx1].grpID=myGroupId;
							strcpy(msgTable[idx1].msg , reply);
							break;
						}
					}
				}
				
			} //todo : check what happens if more than 5 groups are created
		}
		else
		{
			int n = send(fd,"No room to add group\n",22,0);
			printf("Output: Client:%ld: No room to add group\n",myId);  
			if (n <= 0) 
				error("ERROR writing to socket");
		}
		return;
	}
	int n = send(fd,"Invalid Syntax\n",16,0);
	printf("Output: Client:%ld: Invalid Syntax\n",myId);  
	if (n <= 0) 
		error("ERROR writing to socket");
}
void fjoinGroup(int len,char buffer[len],int fd,long myId)
{
	char command[50],dummy[100];
	int grp;
	int ret = sscanf(buffer,"%s %d %[^\n]",command,&grp,dummy);
	if(ret==2)	
	{ // first check if user is already a part of group
		if(checkIfExistGrpid(grp,myId)!=-1)
		{
			int n = send(fd,"Already part of group\n",23,0);  
			if (n <= 0) 
				error("ERROR writing to socket");
			printf("Output: client: %ld Already part of group\n",myId);
			return;
		}
		int groupNo = checkIfExistGrpidV2(grp,myId); // checks user has a pending req or not
		if(groupNo>-1)
		{//user has a pending request
			grpTable[groupNo].members[grpTable[groupNo].memberCount]=myId;
			grpTable[groupNo].memberCount++;
			editMemberInfo(myId,grp,0);
			int n = send(fd,"Added to group\n",16,0);  
			if (n <= 0) 
				error("ERROR writing to socket");
			printf("Output: client: %ld is added to the group %d via /joingroup\n",myId,grp);
			//'client added' message should be sent to all the group members
			char repMsg[100];
			bzero(repMsg,100);
			sprintf(repMsg,"/sendgroup %d Client %ld joined the group %d using /joingroup",grp,myId,grp);
			fsendGroup(strlen(repMsg),repMsg,fd,myId);
		}
		else if(groupNo==-1)
		{
			int n = send(fd,"Invalid Group no\n",18,0);  
			if (n <= 0) 
				error("ERROR writing to socket");
			printf("Output: client: %ld : Invalid Group no\n",myId);
		}
		else if(groupNo==-2)
		{// user is not a part of group so send admin a request to join 
			char requestMsg[100];
			int pp;
			long adm;
			for(pp=0;pp<MAX_GROUP_LIMIT;pp++)
			{
				if(grpTable[pp].grpID==grp)
				{
					adm = grpTable[pp].admin;
					sprintf(requestMsg,"/send %ld Client %ld requested to join group %d, Type '/allow %ld %d' to allow\n",adm,myId,grp,myId,grp);
					break;
				}
			}
			fSend(strlen(requestMsg),requestMsg,fd,myId);
			//add request to reqTable 
			int indexx;
			for(indexx=0;indexx<20;indexx++)
			{
				if(reqTable[indexx].status==EMPTY)
				{
					reqTable[indexx].status=FULL;
					reqTable[indexx].grp=grp;
					reqTable[indexx].admin = adm;
					reqTable[indexx].reqFor = myId;
					break;
				}
			}
			int n = send(fd,"Request Sent to Admin\n",23,0);  
			if (n <= 0) 
				error("ERROR writing to socket");
			printf("Output: client: %ld : Request Sent to Admin\n",myId);			
		}
	}
	else
	{
		int n = send(fd,"Invalid Syntax\n",16,0);
		printf("Output: Client:%ld: Invalid Syntax\n",myId);  
		if (n <= 0) 
			error("ERROR writing to socket");
	}
}
void fQuit(int len,char buffer[len],int fd,long myId,int childPID)
{
	char command[50],dummy[100];
	int ret = sscanf(buffer,"%s %[^\n]",command,dummy);
	if(ret==1)
	{
		int i;
		for(i=0;i<MAX_CLIENT_LIMIT;i++)
		{
			if(clientInfo[i].id==myId)
			{// delete info about this client from all groups and delete groups with this client as admin
				int j,k;
				for(k=0;k<MAX_GROUP_LIMIT;k++)
				{
					int group = clientInfo[i].groups[k];
					if(group!=-1)
					{
						for(j=0;j<ROWS_FOR_GRP_TABLE;j++)
						{
							if(group ==grpTable[j].grpID)
							{
								if(grpTable[j].admin==myId)//delete this group
								{
									char temp[100];
									sprintf(temp,"/sendgroup %d This group is terminated\n",group);
									fsendGroup(strlen(temp),temp,fd,myId);
									//remove all members from this group
									int c;
									for(c=0;c<MAX_CLIENT_LIMIT;c++)
									{
										long member = grpTable[j].members[c];
										if(member!=0)
										{
											removeMemberFromGroup(member,group,myId);
										}
									}
									//invalidating group
									grpTable[j].status=EMPTY;
									grpTable[j].grpID=0;
									grpTable[j].admin = 0;
									int p;
									for(p=0;p<MAX_CLIENT_LIMIT;p++)
									{
										grpTable[j].members[p]=0;
									}

								}
								else
								{ // this client is just a member of the group
									int p;
									for(p=0;p<MAX_CLIENT_LIMIT;p++)
									{
										if(grpTable[j].members[p]==myId)
										{
											grpTable[j].members[p]=0;
											grpTable[j].memberCount--;
											break;
										}
									}
								}
							}
							break;
						}
					}
				}
				clientInfo[i].id=0;
				clientInfo[i].grpCount=0;
				clientInfo[i].adminCount=0;
				int nn;
				for(nn=0;nn<MAX_GROUP_LIMIT;nn++)
				{
					clientInfo[i].groups[nn]=-1;
					clientInfo[i].adminOf[nn]=-1;
				}

				break;
			}
		}
		//broadcast quit msg
		if(*noOfClients>1)
		{
			char msg1[100];
			sprintf(msg1,"/broadcast left chat\n");
			fBroadcast(strlen(msg1),msg1,fd,myId); 
		}
		//delete its child 
		kill(childPID, SIGKILL);
		kill(getpid(),SIGKILL);
	}
	else
	{
		int n = send(fd,"Invalid Syntax\n",16,0);
		printf("Output: Client:%ld: Invalid Syntax\n",myId);  
		if (n <= 0) 
			error("ERROR writing to socket");
	}
}
void fActiveAllGroups(int len,char buffer[len],int fd,long myId)
{
	char command[50],dummy[100];
	int ret = sscanf(buffer,"%s %[^\n]",command,dummy);
	if(ret==1)
	{
		char temp[1000];
		bzero(temp,1000);
		int i,flag=0;
		for(i=0;i<MAX_GROUP_LIMIT;i++)
		{			
			if(grpTable[i].status==FULL)
			{
				flag=1;
				char temp2[500];
				bzero(temp2,500);
				sprintf(temp2,"Group: %d\nMembers: ",grpTable[i].grpID);
				int j;
				for( j=0;j<MAX_CLIENT_LIMIT;j++)
				{
					if(grpTable[i].members[j]!=0)
					{
						char temp3[10];
						bzero(temp3,10);
						sprintf(temp3,"%ld ",grpTable[i].members[j]);
						strcat(temp2,temp3);
					}

				}
				strcat(temp,temp2);
				strcat(temp,"\n");
				char temp4[20];
				bzero(temp4,20);
				sprintf(temp4 , "Admin: %ld",grpTable[i].admin);
				strcat(temp,temp4);
				strcat(temp,"\n\0");
			}
		}
		if(flag==0)
		{
			int n = send(fd,"No Active Groups\n",18,0);
			printf("Output: Client:%ld: No active groups\n",myId);  
			if (n <= 0) 
				error("3ERROR writing to socket");
		}
		else
		{
			int n = send(fd,temp,strlen(temp),0);
			printf("Output: Client:%ld: %s",myId,temp);  
			if (n <= 0) 
				error("ERROR writing to socket");			
		}
	}	
	else
	{
		int n = send(fd,"Invalid Syntax\n",16,0);
		printf("Output: Client:%ld: Invalid Syntax\n",myId);  
		if (n <= 0) 
			error("1ERROR writing to socket");
	}
}
void fAllow(int len,char buffer[len],int fd,long myId)
{
	char command[50],dummy[100];
	long clientRequested;
	int groupRequested;
	int ret = sscanf(buffer,"%s %ld %d %[^\n]",command,&clientRequested,&groupRequested, dummy);
	if(ret==3)
	{
		if(checkIfExist(clientRequested))
		{
			if(checkIfExistGrpidV2(groupRequested,10)==-2)
			{// add user to grp if request is there
				int b;
				for(b=0;b<20;b++)
				{
					if(reqTable[b].status!=EMPTY)
					{
						if(reqTable[b].admin==myId&&reqTable[b].reqFor==clientRequested&&reqTable[b].grp==groupRequested)
						{ // add user to group
							int h;
							for(h=0;h<MAX_GROUP_LIMIT;h++)
							{
								if(grpTable[h].status==FULL&&grpTable[h].grpID==groupRequested)
								{
									if(grpTable[h].memberCount<MAX_GROUP_LIMIT)
									{
										grpTable[h].memberCount++;
										int g;
										for(g=0;g<MAX_GROUP_LIMIT;g++)
										{
											if(grpTable[h].members[g]==0)
											{
												grpTable[h].members[g] = clientRequested;
												int f;
												for(f=0;f<MAX_CLIENT_LIMIT;f++)
												{
													if(clientInfo[f].id==clientRequested)
													{
														clientInfo[f].grpCount++;
														int d;
														for(d=0;d<MAX_GROUP_LIMIT;d++)
														{
															if(clientInfo[f].groups[d]==0)
															{
																clientInfo[f].groups[d]=groupRequested;
																char reqAcc[150];
																bzero(reqAcc,150);
																sprintf(reqAcc,"/sendgroup %d Client %ld is now added to the group %d\n",groupRequested, clientRequested, groupRequested);
																fsendGroup(strlen(reqAcc),reqAcc,fd,myId);															
																break;
															}
															
														}
														break;
													}
												}
												reqTable[b].status=EMPTY;
												break;
											}
										}
									}
									else
									{
										reqTable[b].status=EMPTY;
										int n = send(fd,"No room to add members in group\n",33,0);
										printf("Output: Client:%ld: No room to add members in group\n",myId);  
										if (n <= 0) 
											error("ERROR writing to socket");
									}									
									break; // check this
								}
							}
							return;
						}
					}
				}
				// not entiteled
				reqTable[b].status=EMPTY;
				int n = send(fd,"You are not entitled for this request\n",39,0);
				printf("Output: Client:%ld: You are not entitled for this request\n",myId);  
				if (n <= 0) 
					error("ERROR writing to socket");
			}
			else
			{
				//group donot exist
				int n = send(fd,"Invalid Group ID \n",19,0);
				printf("Output: Client:%ld: Invalid Group ID\n",myId);  
				if (n <= 0) 
					error("ERROR writing to socket");
			}
		}
		else
		{ // invalid client id
			int n = send(fd,"Invalid client ID \n",20,0);
			printf("Output: Client:%ld: Invalid client ID\n",myId);  
			if (n <= 0) 
				error("ERROR writing to socket");
		}
	}
	else
	{ 
		int n = send(fd,"Invalid Syntax\n",16,0);
		printf("Output: Client:%ld: Invalid Syntax\n",myId);  
		if (n <= 0) 
			error("ERROR writing to socket");
	}

}
void performDesiredOperation(int len,char buffer[len],int fd,long clientId,int clientPID)
{
	printf("Command by Client %ld : %s\n",clientId,buffer);
	char command[50],dummy[250];
	sscanf(buffer,"%s %[^\n]",command,dummy);
	if(!strcmp(command,"/active"))
		fActiveClients(len,buffer,fd,clientId);
	else if(!strcmp(command,"/send"))
		fSend(len,buffer,fd,clientId);
	else if(!strcmp(command,"/broadcast"))
		fBroadcast(len,buffer,fd,clientId);
	else if(!strcmp(command,"/makegroup"))
		FmakeGroup(len,buffer,fd,clientId);
	else if(!strcmp(command,"/sendgroup"))
		fsendGroup(len,buffer,fd,clientId);
	else if(!strcmp(command,"/activegroups"))
		fActiveGroups(len,buffer,fd,clientId);
	else if(!strcmp(command,"/makegroupreq"))
		fmakeGroupReq(len,buffer,fd,clientId);
	else if(!strcmp(command,"/joingroup"))	
		fjoinGroup(len,buffer,fd,clientId);
	else if(!strcmp(command,"/quit"))
		fQuit(len,buffer,fd,clientId,clientPID);
	else if(!strcmp(command,"/activeallgroups"))
		fActiveAllGroups(len,buffer,fd,clientId);
	else if(!strcmp(command,"/allow"))
		fAllow(len,buffer,fd,clientId);
	else
	{
		int n = send(fd,"Invalid Syntax\n",16,0);
		printf("Output: Client:%ld: Invalid Syntax\n",clientId);  
		if (n <= 0) 
			error("ERROR writing to socket");
	}
	bzero(buffer,strlen(buffer));
	fflush(stdin);
	fflush(stdout);
}
int main(int argc, char *argv[])
{
	long clientId=10000;
	signal(SIGINT, handle_sigint); 
	int processid=1;
	shmid = shmget(IPC_PRIVATE, 1*sizeof(int), 0777|IPC_CREAT); // to store no of active clients
	groupno = shmget(IPC_PRIVATE, 1*sizeof(int), 0777|IPC_CREAT);
	noOfClients =  (int *) shmat(shmid, 0, 0);
	groupId =  (int *) shmat(shmid, 0, 0);
	*noOfClients=0;   // initializing no of clients to 0
	*groupId=0;       // initializing groupID to 0

	msgTableid = shmget(IPC_PRIVATE, ROWS_FOR_MSG_TABLE*sizeof(msg_table), 0700|IPC_CREAT); // message space array
	grpTableid = shmget(IPC_PRIVATE, ROWS_FOR_GRP_TABLE*sizeof(grp_table), 0700|IPC_CREAT);
	activeClientsid = shmget(IPC_PRIVATE, MAX_CLIENT_LIMIT*sizeof(client_info), 0700|IPC_CREAT);
	reqId = shmget(IPC_PRIVATE, 20*sizeof(req_table), 0700|IPC_CREAT);
	
	msgTable  = (msg_table *)shmat(msgTableid,0,0);
	grpTable  = (grp_table *)shmat(grpTableid,0,0);
	clientInfo = (client_info *)shmat(activeClientsid,0,0);
	reqTable = (req_table *)shmat(reqId,0,0);
	clearAllMemoryAreas(); // set all shared variables to 0

	struct sembuf pop, vop ;
	semid1 = semget(IPC_PRIVATE, 1, 0777|IPC_CREAT);  // semaphore for file operations
	semid2 = semget(IPC_PRIVATE, 1, 0777|IPC_CREAT);  // semaphore for active clients
	semctl(semid1, 0, SETVAL, 1);  // initialization of semaphores
	semctl(semid2, 0, SETVAL, 1);
	pop.sem_num = vop.sem_num = 0;
	pop.sem_flg = vop.sem_flg = 0;
	pop.sem_op = -1 ; vop.sem_op = 1 ;
	int sockfd, newsockfd, portno, clilen, n;
	char buffer[1024];
	portno=PORT;
	int opt=1;
	struct sockaddr_in serv_addr, cli_addr;
	sockfd = socket(AF_INET, SOCK_STREAM, 0); // socket connection
	if (sockfd < 0)
		error("ERROR opening socket");
	bzero((char *) &serv_addr, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(portno);
	serv_addr.sin_addr.s_addr = INADDR_ANY;
	if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt))) //to reuse same port again
	{ 
        printf("setsockopt");  
    } 
	if (bind(sockfd, (struct sockaddr *) &serv_addr,sizeof(serv_addr)) < 0)
		error("ERROR on binding");
	printf("Socket Connection Established..\n");
	listen(sockfd,10);
	clilen = sizeof(cli_addr);
	// todo: if a '/makegroup 10001 10002 ' is typed it generates an error ( a space after 10002)	
	while(1) //loop for new clients
	{
		newsockfd = accept(sockfd, (struct sockaddr *) &cli_addr, &clilen);
		clientId++;
		int i,flag =0;
		for(i=0;i<MAX_CLIENT_LIMIT;i++)
		{
			if(clientInfo[i].id==0)
			{
				flag =1;
				clientInfo[i].id=clientId;
				clientInfo[i].grpCount=0;
				clientInfo[i].adminCount=0;
				int c;
				for(c=0;c<MAX_GROUP_LIMIT;c++)
				{
					clientInfo[i].adminOf[c]=0;
					clientInfo[i].groups[c]=0;
				} 
				break;
			}
		}
		if(flag==0)
		{
			printf("No room to place client.\n");
		}
		P(semid2); // critical section for increasing no of clients
			*noOfClients = *noOfClients+1;
		V(semid2);
		if (newsockfd < 0)
			error("ERROR on accept");
		int pid = fork();  // forking a new process of each client
		if( pid !=0)
		{	// parent		
			close(newsockfd);
			continue;
		}
		else // new process to handle new client
		{
			bzero(buffer,1024);
			if(*noOfClients>MAX_CLIENT_LIMIT)
			{
				*noOfClients=*noOfClients-1;
				n = send(newsockfd,"Max Client Limit Exceeded..\n",29,0);  //Sending Ack to Client
				if (n <= 0) 
					error("ERROR writing to socket");
				exit(0);				
			}
			else
			{
				char welcome[100];
				sprintf(welcome,"Connected To Server, Id: %ld",clientId);
				n = write(newsockfd,welcome,256);  //Sending Ack to Client
				if (n <= 0) 
					error("ERROR writing to socket");
			}
			printf("New Client Connected, ID : %ld\n",clientId);
			int childPid =fork();
			if(childPid !=0)// process to handle the commands from client
			{
				while(1)  // loops till client closes the connection
				{				
					char bufferRead[1024];
					printf("Waiting For Command From Client..\n");
					bzero(bufferRead,strlen(bufferRead));
					n = recv(newsockfd,bufferRead,1023,0); 
					if (n <= 0) // when some client disconnects
					{
						P(semid2);
							*noOfClients =*noOfClients-1;
						V(semid2);
						printf("Client Disconnected..\n");
						exit(0); // if client disconnects , forked process exits
					}
					int len = strlen(bufferRead);
					performDesiredOperation(len,bufferRead,newsockfd,clientId,childPid);	 
				}
			}
			else // process to handle the messages meant for client
			{
				while(1)
				{
					char dataToBeWritten[1024];
					int retval = performReading(strlen(dataToBeWritten),dataToBeWritten,clientId);
					if(retval) // if there is something to be written 
					{
						n = send(newsockfd,dataToBeWritten,strlen(dataToBeWritten),0);
						if(n<=0)
						{
							error("ERROR writing to socket");
						}
					}
				}
			}
		}
	}
	return 0;
}	