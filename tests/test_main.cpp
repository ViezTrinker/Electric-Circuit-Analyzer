/*!
 * \file test_main.cpp
 * \brief Test runner for the circuit analyzer core.
 */

#include <iostream>

int RunAnalysisTests(void);
int RunDigitalTests(void);

int main(void)
{
   int failures = RunAnalysisTests() + RunDigitalTests();
   if (failures == 0)
   {
      std::cout << "All tests passed.\n";
      return 0;
   }
   std::cout << failures << " test failure(s).\n";
   return 1;
}
