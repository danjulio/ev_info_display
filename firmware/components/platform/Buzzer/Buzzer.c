#include "Buzzer.h"
#include "TCA9554PWR.h"

void Buzzer_On(void)
{
    (void) Set_EXIO(TCA9554_EXIO8, true);
}

void Buzzer_Off(void)
{
    (void) Set_EXIO(TCA9554_EXIO8, false);
}