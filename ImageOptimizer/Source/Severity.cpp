#include "Severity.h"

#include <string>

const Severity::severity_array_t Severity::s_severityNames =
{
	"TRACE",
	"DEBUG",
	"INFO",
	"WARNING",
	"ERROR",
	"FATAL"
};

const SeverityLevel Severity::s_defaultSeverityLevel = SeverityLevel::trace;
const std::string& Severity::s_defaultSeverityLevelName = s_severityNames[static_cast<array_size_t>( s_defaultSeverityLevel)];

const std::string& Severity::GetSeverityLevelName(SeverityLevel severity)
{
	if (static_cast<int>(severity) < 0)
	{
		return s_defaultSeverityLevelName;
	}

	array_size_t severityIndex =  static_cast<array_size_t>(severity);

	if( severityIndex < s_severityNames.size() )
	{
		return s_severityNames[severityIndex];
	}
	else
	{
		return s_defaultSeverityLevelName;
	}
}

template <typename T1, typename T2>
bool iequals(const T1& collection1, const T2& collection2)
{
	for (size_t i = 0; i < collection1.size(); i++)
	{
		if (collection1[i] != collection2[i])
		{
			return false;
		}
	}

	return true;
}

SeverityLevel Severity::GetSeverityLevel(const std::string& severityName)
{	
	for (array_size_t i = 0; i < s_severityNames.size(); i++)
	{
		if (iequals(severityName, s_severityNames[i]))
		{
			return static_cast<SeverityLevel>(i);
		}
	}

	return s_defaultSeverityLevel;
}

bool Severity::IsValid(const std::string& severityName)
{	
	for(const auto& name : s_severityNames)
	{
		if (iequals(severityName, name))
		{
			return true;
		}
	}

	return false;
}
