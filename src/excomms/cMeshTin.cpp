/*
 * File		: cMeshTin.h
 * Purpose	: Implementation of the TIN codec
 * Data		: 18/11/2022
 */
#include "stdafx.h"

#ifdef _WIN64
namespace comms {
#include "cMeshTin.h"
}
#include "TIN\tinExporter.h"

namespace comms {
	cMeshTin::cMeshTin(){}
	
	cMeshTin::~cMeshTin(){}

	comms::cMeshContainer* cMeshTin::Decode(comms::cData& data) {
		return NULL;
	}

	void cMeshTin::Encode(comms::cMeshContainer& mesh, comms::cData* data) {
		if (nullptr != data) {
			fileTType ftype = cStr::Equals(data->GetFilePn().GetFileExtension(), TIN_EXTENSION) ? FT_Binary : FT_XML;
			tin::tinExporter* exporter = new tin::tinExporter(&mesh, data);
			exporter->setFormatType(ftype);
			exporter->Export();
			delete exporter;
		}
	}
}
#endif // _WIN64
