/*!
 * \file test_main.cpp
 * \brief Test runner for the circuit analyzer core.
 */

#include <iostream>

int RunAnalysisTests(void);
int RunDigitalTests(void);
int RunMixedTests(void);
int RunDeviceTests(void);
int RunFileTests(void);
int RunExampleTests(void);

int main(void)
{
   int failures = RunAnalysisTests() + RunDigitalTests() + RunMixedTests() + RunDeviceTests() + RunFileTests() + RunExampleTests();
   if (failures == 0)
   {
      std::cout << "All tests passed.\n";
      return 0;
   }
   std::cout << failures << " test failure(s).\n";
   return 1;
}
