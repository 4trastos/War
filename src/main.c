#include "../incl/war.h"

int main(void)
{
    t_war   *war;

    if (ptrace(PTRACE_TRACEME, 0, 0, 0) == -1)
        exit(1);
    war = malloc(sizeof(t_war));
    if (!war)
        return (1);
    return (0);
}