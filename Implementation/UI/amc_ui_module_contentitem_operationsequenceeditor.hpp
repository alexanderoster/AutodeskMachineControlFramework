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


#ifndef __AMC_UI_MODULE_CONTENTITEM_OPERATIONSEQUENCEEDITOR
#define __AMC_UI_MODULE_CONTENTITEM_OPERATIONSEQUENCEEDITOR

#include "header_protection.hpp"

#ifndef __AMCIMPL_UI_MODULE
#error this header is protected and should only be included in the corresponding implementation CPP files.
#endif

#include "amc_ui_module_contentitem.hpp"
#include "amc_operationsequencehandler.hpp"

#include "pugixml.hpp"
#include "RapidJSON/document.h"

#include <set>
#include <vector>

namespace AMC {

	amcDeclareDependingClass(CUIModule_ContentOperationSequenceEditor, PUIModule_ContentOperationSequenceEditor);
	amcDeclareDependingClass(CUIModuleEnvironment, PUIModuleEnvironment);
	amcDeclareDependingClass(CResourcePackage, PResourcePackage);
	amcDeclareDependingClass(CAccessControl, PAccessControl);

	class CUIModule_ContentOperationSequenceEditor : public CUIModule_ContentItem {
	protected:
		std::string m_sStateMachineName;
		std::vector<std::string> m_SequenceFilter;
		std::string m_sEditPermission;
		std::string m_sHighlightExecution;
		std::string m_sSaveEvent;

		POperationSequenceHandler m_pOperationSequenceHandler;
		PAccessControl m_pAccessControl;
		PResourcePackage m_pResourcePackage;
		LibMCData::PDataModel m_pDataModel;

		POperationSequenceRegistry getRegistry();

		bool sequenceIsVisible(const std::string& sSequenceName);
		void checkSequenceIsVisible(const std::string& sSequenceName);
		bool userCanEdit(PAPIAuth pAuth);
		std::string resolveUserName(const std::string& sUserUUID);
		std::string resolveIconResource(const std::string& sResourceName);

		void writeCatalogToJSON(CJSONWriter& writer, CJSONWriterObject& object, const std::string& sName);
		void writeSequencesToJSON(CJSONWriter& writer, CJSONWriterObject& object, const std::string& sName);
		void writeReferencesToJSON(CJSONWriter& writer, CJSONWriterObject& object, const std::string& sName);
		void writeParameterDefinitionsToJSON(CJSONWriter& writer, CJSONWriterObject& object, COperationParameterContainer* pContainer);
		void writeValidationResultToJSON(CJSONWriter& writer, CJSONWriterObject& object, COperationSequenceValidationResult* pResult);

		POperationSequence parseSequenceRequest(const CAPIJSONRequest& requestData);

	public:

		static PUIModule_ContentOperationSequenceEditor makeFromXML(const pugi::xml_node& xmlNode, const std::string& sItemName, const std::string& sModulePath, PUIModuleEnvironment pUIModuleEnvironment);

		CUIModule_ContentOperationSequenceEditor(const std::string& sUUID, const std::string& sItemName, const std::string& sModulePath, PUIModuleEnvironment pUIModuleEnvironment);

		virtual ~CUIModule_ContentOperationSequenceEditor();

		void setStateMachineName(const std::string& sStateMachineName);
		void setSequenceFilter(const std::vector<std::string>& sequenceFilter);
		void setEditPermission(const std::string& sEditPermission);
		void setHighlightExecution(const std::string& sHighlightExecution);
		void setSaveEvent(const std::string& sSaveEvent);

		virtual void populateClientVariables(CParameterHandler* pClientVariableHandler) override;

		virtual std::string findElementPathByUUID(const std::string& sUUID) override;

		virtual std::string getItemType() override;

		virtual void registerFrontendAttributes() override;

		virtual void handleCustomRequest(PAPIAuth pAuth, const std::string& requestType, const CAPIJSONRequest& requestData, CJSONWriter& response, CUIModule_UIEventHandler* pEventHandler) override;

		// JSON form of a sequence, as used by the editor widget.
		static void writeSequenceToJSON(CJSONWriter& writer, CJSONWriterObject& object, COperationSequence* pSequence);
		static POperationSequence parseSequenceFromJSON(const rapidjson::Value& sequenceValue);

	};

}


#endif //__AMC_UI_MODULE_CONTENTITEM_OPERATIONSEQUENCEEDITOR
