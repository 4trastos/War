#include "../incl/war.h"

int main(void)
{
    t_war   *war;
    char    path[1024];
    int     i = 0;

    if (ptrace(PTRACE_TRACEME, 0, 0, 0) == -1)
        exit(1);
    war = malloc(sizeof(t_war));
    if (!war)
        return (1);
    war->antivirus = 0;
    ft_antiprocess(war);

    if (war->antivirus != 1)
    {
        
    }
    return (0);
}