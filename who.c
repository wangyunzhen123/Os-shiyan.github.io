#include<string.h>
#include<errno.h>
#include<asm/segment.h>

char msg[24];

int sys_iam(const char *name) {
   int i;
   char tmp[30];
   for(i = 0; i < 30; i++) {
      //get data from user
      tmp[i] = get_fs_byte(name + i);
      if(tmp[i] == '\0')
         break;
   }
   i = 0;
   while(i < 30 && tmp[i] != '\0')
      i++;
  if(i > 23) {
     //printk("String too long\n");
     return -(EINVAL);
   }
   
  strcpy(msg,tmp);
  return i;
}

int sys_whoami(char *name, unsigned int size) {
  int len = 0;
  for(; msg[len] != '\0'; len++)
       ;
  if(len > size) {
     return -(EINVAL);
   }
  int i = 0;
  for(int i = 0; i < size; i++) {
     put_fs_byte(msg[i], name + i);
     if(msg[i] == '\0')
     break;
  }
  return i
}