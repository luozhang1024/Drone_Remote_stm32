#ifndef APP_TASKS_H
#define APP_TASKS_H
/* 任务入口不能返回；暂时不需要的任务也不能用普通 return 退出。 */
void AppRadioTask(void *argument);
void AppSlowTask(void *argument);
#endif
