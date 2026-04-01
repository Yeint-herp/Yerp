#ifndef SUPERVISOR_EXECUTIVE_DPC_H
#define SUPERVISOR_EXECUTIVE_DPC_H

typedef struct Dpc Dpc;

typedef void (*Dpc_Routine)(Dpc *dpc, void *context, void *arg1, void *arg2);

struct Dpc
{
    Dpc_Routine Routine;
    void       *Context;
    void       *Arg1;
    void       *Arg2;
};

void Dpc_Init(Dpc *dpc, Dpc_Routine routine, void *context);

bool Dpc_Queue(Dpc *dpc, void *arg1, void *arg2);
bool Dpc_QueueTarget(Dpc *dpc, void *arg1, void *arg2, u32 processorNumber);

void Dpc_SystemInit(void);

#endif /* SUPERVISOR_EXECUTIVE_DPC_H */
