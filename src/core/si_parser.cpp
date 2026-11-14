/*!
 * \file si_parser.cpp
 * \brief SI prefix parsing and formatting.
 */

#include "si_parser.h"

#include <cmath>
#include <cstdlib>

namespace Analyzer
{

namespace
{

constexpr double Kilo = 1.0e3;
constexpr double Mega = 1.0e6;
constexpr double Giga = 1.0e9;
constexpr double Tera = 1.0e12;
constexpr double Milli = 1.0e-3;
constexpr double Micro = 1.0e-6;
constexpr double Nano = 1.0e-9;
constexpr double Pico = 1.0e-12;
constexpr double Femto = 1.0e-15;

bool StartsWith(std::string_view text, std::string_view prefix)
{
   if (text.size() < prefix.size())
   {
      return false;
   }
   for (size_t index = 0; index < prefix.size(); index += 1)
   {
      if (text[index] != prefix[index])
      {
         return false;
      }
   }
   return true;
}

double PrefixScale(std::string_view prefix, size_t* pConsumed)
{
   *pConsumed = 0;
   if (prefix.empty())
   {
      return 1.0;
   }
   if (StartsWith(prefix, "Meg"))
   {
      *pConsumed = 3;
      return Mega;
   }
   char first = prefix.front();
   *pConsumed = 1;
   if (first == 'T')
   {
      return Tera;
   }
   if (first == 'G')
   {
      return Giga;
   }
   if (first == 'M')
   {
      return Mega;
   }
   if (first == 'k' || first == 'K')
   {
      return Kilo;
   }
   if (first == 'm')
   {
      return Milli;
   }
   if (first == 'u')
   {
      return Micro;
   }
   if (first == 'n')
   {
      return Nano;
   }
   if (first == 'p')
   {
      return Pico;
   }
   if (first == 'f')
   {
      return Femto;
   }
   *pConsumed = 0;
   return 1.0;
}

} // namespace

Result ParseSiNumber(std::string_view text, double* pValue)
{
   if (pValue == nullptr)
   {
      return Result::InvalidParameter;
   }
   size_t begin = 0;
   while (begin < text.size() && (text[begin] == ' ' || text[begin] == '\t'))
   {
      begin += 1;
   }
   if (begin >= text.size())
   {
      return Result::InvalidParameter;
   }
   std::string numeric(text.substr(begin));
   char* pEnd = nullptr;
   double magnitude = std::strtod(numeric.c_str(), &pEnd);
   if (pEnd == numeric.c_str())
   {
      return Result::InvalidParameter;
   }
   size_t consumedNumber = static_cast<size_t>(pEnd - numeric.c_str());
   std::string_view rest(numeric.c_str() + consumedNumber);
   while (rest.empty() == false && (rest.front() == ' ' || rest.front() == '\t'))
   {
      rest.remove_prefix(1);
   }
   size_t consumedPrefix = 0;
   double scale = PrefixScale(rest, &consumedPrefix);
   rest.remove_prefix(consumedPrefix);
   while (rest.empty() == false && (rest.front() == ' ' || rest.front() == '\t'))
   {
      rest.remove_prefix(1);
   }
   *pValue = magnitude * scale;
   return Result::Ok;
}

std::string FormatSiNumber(double value)
{
   if (std::isfinite(value) == false)
   {
      return "0";
   }
   double sign = 1.0;
   double magnitude = value;
   if (magnitude < 0.0)
   {
      sign = -1.0;
      magnitude = -magnitude;
   }
   const char* pSuffix = "";
   double scaled = magnitude;
   if (magnitude >= Tera)
   {
      scaled = magnitude / Tera;
      pSuffix = "T";
   }
   else if (magnitude >= Giga)
   {
      scaled = magnitude / Giga;
      pSuffix = "G";
   }
   else if (magnitude >= Mega)
   {
      scaled = magnitude / Mega;
      pSuffix = "Meg";
   }
   else if (magnitude >= Kilo)
   {
      scaled = magnitude / Kilo;
      pSuffix = "k";
   }
   else if (magnitude >= 1.0 || magnitude == 0.0)
   {
      scaled = magnitude;
      pSuffix = "";
   }
   else if (magnitude >= Milli)
   {
      scaled = magnitude / Milli;
      pSuffix = "m";
   }
   else if (magnitude >= Micro)
   {
      scaled = magnitude / Micro;
      pSuffix = "u";
   }
   else if (magnitude >= Nano)
   {
      scaled = magnitude / Nano;
      pSuffix = "n";
   }
   else if (magnitude >= Pico)
   {
      scaled = magnitude / Pico;
      pSuffix = "p";
   }
   else
   {
      scaled = magnitude / Femto;
      pSuffix = "f";
   }
   std::string digits = std::to_string(sign * scaled);
   size_t dot = digits.find('.');
   if (dot != std::string::npos)
   {
      while (digits.empty() == false && digits.back() == '0')
      {
         digits.pop_back();
      }
      if (digits.empty() == false && digits.back() == '.')
      {
         digits.pop_back();
      }
   }
   digits += pSuffix;
   return digits;
}

} // namespace Analyzer
