#ifndef _EMPLOYEE_
#define _EMPLOYEE_              

#define MAX_NAME_LENGTH 16
int i=1;

// Hometown 
typedef enum Home
{
    Anhui = 1,
    Beijing=9, 
    Shanghai,
    Zhejiang = 33
}Home;
/**/
typedef struct Manager {
	char a;
    int level;
}Manager;

//*
typedef struct _Engineer
{
	char a;
	char b;
	short c;
    int skills;
}_Engineer;//*/

typedef struct 
{
    char name[MAX_NAME_LENGTH];
    int  age;
    enum Home home;
}Person;

typedef union Position
{
    struct Manager manager;
    //Engineer engineer;
    struct Engineer
	{
		//char a;
		//short b;
	    int skills;
	}engineer;
}Position;

typedef struct Employee
{
    int  id;
    Person  person;
    union Position position;
}Employee;

union _Position{
	int level;
	int skills;
};
#endif