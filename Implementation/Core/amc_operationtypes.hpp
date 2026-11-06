/*++

Copyright (C) 2026 Autodesk Inc.

All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:
	* Redistributions of source code must retain the above copyright
	  notice, this list of conditions and the following disclaimer.
	* Redistributions in binary form must reproduce the above copyright
	  notice, this list of conditions and the following disclaimer in the
	  documentation and/or other materials provided with the distribution.
	* Neither the name of the Autodesk Inc. nor the
	  names of its contributors may be used to endorse or promote products
	  derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL AUTODESK INC. BE LIABLE FOR ANY
DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

*/


#ifndef __AMC_OPERATIONTYPES
#define __AMC_OPERATIONTYPES

#include "header_protection.hpp"

#include <memory>
#include <string>
#include <vector>
#include <map>
#include <set>
#include <mutex>

namespace AMC {

	// The values of these enums are identical to the corresponding LibMCEnv enums.
	enum class eOperationParameterType : uint32_t {
		Unknown = 0,
		String = 1,
		Integer = 2,
		Double = 3,
		Bool = 4,
		Enum = 5
	};

	enum class eOperationExpressionMode : uint32_t {
		LiteralOnly = 1,
		LiteralOrReference = 2,
		Formula = 3
	};

	enum class eOperationValueKind : uint32_t {
		Default = 0,
		Literal = 1,
		Reference = 2,
		Formula = 3
	};

	enum class eOperationSequenceOrigin : uint32_t {
		Unknown = 0,
		Resource = 1,
		String = 2,
		Code = 3,
		Editor = 4,
		Document = 5
	};

	enum class eOperationSequenceExecutionStatus : uint32_t {
		Running = 1,
		Completed = 2,
		Aborted = 3,
		Failed = 4
	};

	enum class eOperationSequenceIssueSeverity : uint32_t {
		Warning = 1,
		Error = 2
	};

	class COperationParameterDefinition;
	class COperationParameterContainer;
	class COperationItemListDefinition;
	class COperationTypeDefinition;

	typedef std::shared_ptr<COperationParameterDefinition> POperationParameterDefinition;
	typedef std::shared_ptr<COperationParameterContainer> POperationParameterContainer;
	typedef std::shared_ptr<COperationItemListDefinition> POperationItemListDefinition;
	typedef std::shared_ptr<COperationTypeDefinition> POperationTypeDefinition;

	class COperationParameterDefinition {
	private:
		std::string m_sName;
		std::string m_sDisplayName;
		std::string m_sDescription;
		eOperationParameterType m_ParameterType;
		std::string m_sDefaultValue;
		std::string m_sUnit;
		bool m_bHasRange;
		double m_dMinimum;
		double m_dMaximum;
		eOperationExpressionMode m_ExpressionMode;
		bool m_bRequired;
		std::vector<std::pair<std::string, std::string>> m_EnumOptions;

		mutable std::mutex m_Mutex;

	public:

		COperationParameterDefinition(const std::string& sName, const std::string& sDisplayName, eOperationParameterType parameterType, const std::string& sDefaultValue, bool bHasRange, double dMinimum, double dMaximum);

		virtual ~COperationParameterDefinition();

		std::string getName() const;

		std::string getDisplayName() const;
		void setDisplayName(const std::string& sDisplayName);

		std::string getDescription() const;
		void setDescription(const std::string& sDescription);

		eOperationParameterType getParameterType() const;

		bool isNumeric() const;
		bool isStringLike() const;

		std::string getDefaultValue() const;

		std::string getUnit() const;
		void setUnit(const std::string& sUnit);

		bool hasRange() const;
		void getRange(double& dMinimum, double& dMaximum) const;

		// Clamps the value to the range, if any.
		double clampValue(double dValue) const;

		eOperationExpressionMode getExpressionMode() const;
		void setExpressionMode(eOperationExpressionMode expressionMode);

		bool isRequired() const;
		void setRequired(bool bRequired);

		void addEnumOption(const std::string& sValue, const std::string& sDisplayName);
		uint32_t getEnumOptionCount() const;
		void getEnumOption(uint32_t nIndex, std::string& sValue, std::string& sDisplayName) const;
		bool hasEnumOption(const std::string& sValue) const;
		std::vector<std::pair<std::string, std::string>> getEnumOptions() const;

		bool hasSameDefinition(const std::string& sDisplayName, eOperationParameterType parameterType, const std::string& sDefaultValue, bool bHasRange, double dMinimum, double dMaximum) const;

		static std::string parameterTypeToString(eOperationParameterType parameterType);
		static std::string expressionModeToString(eOperationExpressionMode expressionMode);
	};

	class COperationParameterContainer {
	private:
		std::vector<POperationParameterDefinition> m_Parameters;
		std::map<std::string, POperationParameterDefinition> m_ParameterMap;

	protected:
		mutable std::mutex m_ContainerMutex;

		// Returns true if the name collides with a name of the container, e.g. an item list.
		virtual bool nameIsReserved(const std::string& sName);

		POperationParameterDefinition addParameter(const std::string& sName, const std::string& sDisplayName, eOperationParameterType parameterType, const std::string& sDefaultValue, bool bHasRange, double dMinimum, double dMaximum);

	public:

		COperationParameterContainer();

		virtual ~COperationParameterContainer();

		POperationParameterDefinition addStringParameter(const std::string& sName, const std::string& sDisplayName, const std::string& sDefaultValue);
		POperationParameterDefinition addIntegerParameter(const std::string& sName, const std::string& sDisplayName, int64_t nDefaultValue, int64_t nMinimum, int64_t nMaximum);
		POperationParameterDefinition addDoubleParameter(const std::string& sName, const std::string& sDisplayName, double dDefaultValue, double dMinimum, double dMaximum);
		POperationParameterDefinition addBoolParameter(const std::string& sName, const std::string& sDisplayName, bool bDefaultValue);
		POperationParameterDefinition addEnumParameter(const std::string& sName, const std::string& sDisplayName, const std::string& sDefaultValue);

		bool hasParameter(const std::string& sName) const;
		POperationParameterDefinition findParameter(const std::string& sName, bool bFailIfNotExisting) const;
		std::vector<POperationParameterDefinition> getParameters() const;

		static bool isReservedAttributeName(const std::string& sName);
		static bool isValidIdentifier(const std::string& sName);
	};

	class COperationItemListDefinition : public COperationParameterContainer {
	private:
		std::string m_sName;
		std::string m_sDisplayName;
		uint32_t m_nMaxItemCount;

	public:

		COperationItemListDefinition(const std::string& sName, const std::string& sDisplayName, uint32_t nMaxItemCount);

		virtual ~COperationItemListDefinition();

		std::string getName() const;
		std::string getDisplayName() const;
		uint32_t getMaxItemCount() const;
	};

	class COperationTypeDefinition : public COperationParameterContainer {
	private:
		std::string m_sName;
		std::string m_sTargetState;
		std::string m_sDisplayName;
		std::string m_sDescription;
		std::string m_sCategory;
		std::string m_sIconResource;
		std::string m_sAllowedContexts;
		std::set<std::string> m_AllowedContextSet;

		std::vector<POperationItemListDefinition> m_ItemLists;
		std::map<std::string, POperationItemListDefinition> m_ItemListMap;

	protected:
		bool nameIsReserved(const std::string& sName) override;

	public:

		COperationTypeDefinition(const std::string& sName, const std::string& sTargetState);

		virtual ~COperationTypeDefinition();

		std::string getName() const;
		std::string getTargetState() const;

		std::string getDisplayName() const;
		void setDisplayName(const std::string& sDisplayName);

		std::string getDescription() const;
		void setDescription(const std::string& sDescription);

		std::string getCategory() const;
		void setCategory(const std::string& sCategory);

		std::string getIconResource() const;
		void setIconResource(const std::string& sIconResource);

		std::string getAllowedContexts() const;
		std::set<std::string> getAllowedContextSet() const;
		void setAllowedContexts(const std::string& sContexts);
		bool isAllowedInContext(const std::string& sContext) const;

		POperationItemListDefinition addItemList(const std::string& sName, const std::string& sDisplayName, uint32_t nMaxItemCount);
		POperationItemListDefinition findItemList(const std::string& sName, bool bFailIfNotExisting) const;
		std::vector<POperationItemListDefinition> getItemLists() const;
	};

}

#endif //__AMC_OPERATIONTYPES
