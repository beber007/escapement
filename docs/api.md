# API

An application is built in five steps, the last one handing control to the
kernel for good:

```c
void Task1(void *argument);

int main(void)
{
   /* (1) processor-specific initialisation                                  */
   /* (2) application initialisation                                         */
   /* (3) periodic task: period and deadline of 10000 ticks, argument 34     */
   OSCreateTask(Task1, 0, 10000, 10000, (void *)34);
   /* (4) core clock source and frequency — see the bundled examples         */
   return OSStartMultitasking(NULL, NULL);   /* (5) never returns            */
}

void Task1(void *argument)
{
   /* body of the task */
   OSEndTask();                              /* mandatory at the end        */
}
```

The complete interface is documented in the `EscapementHard.h`,
`EscapementSoft.h` and `EscapementHardPA.h` headers.
