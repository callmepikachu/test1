#include "stdio.h"
#include <sys/shm.h>
#include <sys/sem.h>
#include <stdlib.h>
#define SHM_SIZE (1024*1024)
#define SHM_MODE 0600
#define SEM_MODE 0600
 
#if defined(__GNU_LIBRARY__) && !defined(_SEM_SEMUN_UNDEFINED)
/*   union   semun   is   defined   by   including   <sys/sem.h>   */ 
#else 
/*   according   to   X/OPEN   we   have   to   define   it   ourselves   */ 
union semun{
	int val;
	struct semid_ds *buf;
	unsigned short *array;
};
#endif
 
struct ShM{
	int start;
	int end;
}* pSM;
 
const int N_CONSUMER = 3;//����������
const int N_BUFFER = 5;//����������
int shmId = -1,semSetId=-1;
union semun su;//sem union�����ڳ�ʼ���ź���
 
//semSetId ��ʾ�ź������ϵ� id
//semNum ��ʾҪ������ź������ź��������е�����
void waitSem(int semSetId,int semNum)
{
	struct sembuf sb;
	sb.sem_num = semNum;
	sb.sem_op = -1;//��ʾҪ���ź�����һ
	sb.sem_flg = SEM_UNDO;//
	//�ڶ��������� sembuf [] ���͵ģ���ʾ����
	//������������ʾ �ڶ����������������Ĵ�С
	if(semop(semSetId,&sb,1) < 0){
		perror("waitSem failed");
		exit(1);
	}
}
void sigSem(int semSetId,int semNum)
{
	struct sembuf sb;
	sb.sem_num = semNum;
	sb.sem_op = 1;
	sb.sem_flg = SEM_UNDO;
	//�ڶ��������� sembuf [] ���͵ģ���ʾ����
	//������������ʾ �ڶ����������������Ĵ�С
	if(semop(semSetId,&sb,1) < 0){
		perror("waitSem failed");
		exit(1);
	}
}
//�����ڱ�֤�����Լ�����������������µ���
void produce()
{
	int last = pSM->end;
	pSM->end = (pSM->end+1) % N_BUFFER;
	printf("produce %d\n",last);
}
//�����ڱ�֤�����Լ����������յ�����µ���
void consume()
{
	int last = pSM->start;
	pSM->start = (pSM->start + 1)%N_BUFFER;
	printf("consume %d\n",last);
}
 
void init()
{
	//�����������Լ���ʼ��
	if((shmId = shmget(IPC_PRIVATE,SHM_SIZE,SHM_MODE)) < 0)
	{
		perror("create shared memory failed");
		exit(1);
	}
	pSM = (struct ShM *)shmat(shmId,0,0);
	pSM->start = 0;
	pSM->end = 0;
	
	//�ź�������
	//��һ��:ͬ���ź���,��ʾ�Ⱥ�˳��,�����пռ��������
	//�ڶ���:ͬ���ź���,��ʾ�Ⱥ�˳��,�����в�Ʒ��������
	//������:�����ź���,�����ߺ�ÿ�������߲���ͬʱ���뻺����
 
	if((semSetId = semget(IPC_PRIVATE,3,SEM_MODE)) < 0)
	{
		perror("create semaphore failed");
		exit(1);
	}
	//�ź�����ʼ��,���� su ��ʾ union semun 
	su.val = N_BUFFER;//��ǰ�ⷿ�����Խ��ն��ٲ�Ʒ
	if(semctl(semSetId,0,SETVAL, su) < 0){
		perror("semctl failed");
		exit(1);
	}
	su.val = 0;//��ǰû�в�Ʒ
	if(semctl(semSetId,1,SETVAL,su) < 0){
		perror("semctl failed");
		exit(1);
	}
	su.val = 1;//Ϊ1ʱ���Խ��뻺����
	if(semctl(semSetId,2,SETVAL,su) < 0){
		perror("semctl failed");
		exit(1);
	}
}
int main()
{
	int i = 0,child = -1;
	init();
	//���� �����N_CONSUMER���������ӽ���
	for(i = 0; i < N_CONSUMER; i++)
	{
		if((child = fork()) < 0)//����forkʧ��
		{
			perror("the fork failed");
			exit(1);
		}
		else if(child == 0)//�ӽ���
		{
			printf("NO.%d consumer��PID = %d\n",i,getpid());
			while(1)
			{
				waitSem(semSetId,1);//�����в�Ʒ��������
				waitSem(semSetId,2);//����������
				consume();//��ò�Ʒ,��Ҫ�޸Ļ�����
				sigSem(semSetId,2);//�ͷŻ�����
				sigSem(semSetId,0);//��֪������,�пռ���
				sleep(2);//����Ƶ��
			}
			break;//�����
		}
	}
	
	
	//�����̿�ʼ����
	if(child > 0)
	{
		while(1)
		{
			waitSem(semSetId,0);//��ȡһ���ռ����ڴ�Ų�Ʒ
			waitSem(semSetId,2);//ռ�в�Ʒ������
			produce();
			sigSem(semSetId,2);//�ͷŲ�Ʒ������
			sleep(1);//ÿ��������һ��
			sigSem(semSetId,1);//��֪�������в�Ʒ��
		}
	}
	return 0;
}
