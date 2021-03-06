/*******************************************************************************
 *  Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 *  The contents of this file are subject to the Mozilla Public License
 *  Version 1.1 (the "License"); you may not use this file except in
 *  compliance with the License. You may obtain a copy of the License at
 *  http://www.mozilla.org/MPL/
 *
 *  Software distributed under the License is distributed on an "AS IS"
 *  basis, WITHOUT WARRANTY OF ANY KIND, either express or implied. See the
 *  License for the specific language governing rights and limitations
 *  under the License.
 *
 *  The Original Code is ICMA
 *
 *  The Initial Developer of the Original Code is University of Auckland,
 *  Auckland, New Zealand.
 *  Copyright (C) 2007-2010 by the University of Auckland.
 *  All Rights Reserved.
 *
 *  Contributor(s): Jagir R. Hussan
 *
 *  Alternatively, the contents of this file may be used under the terms of
 *  either the GNU General Public License Version 2 or later (the "GPL"), or
 *  the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 *  in which case the provisions of the GPL or the LGPL are applicable instead
 *  of those above. If you wish to allow use of your version of this file only
 *  under the terms of either the GPL or the LGPL, and not to allow others to
 *  use your version of this file under the terms of the MPL, indicate your
 *  decision by deleting the provisions above and replace them with the notice
 *  and other provisions required by the GPL or the LGPL. If you do not delete
 *  the provisions above, a recipient may use your version of this file under
 *  the terms of any one of the MPL, the GPL or the LGPL.
 *
 * "2014"
 *******************************************************************************/
#include "RCMeshFitting.h"
#include "refinedrcheart.h"

int RCMeshFitting::id = 0;

RCMeshFitting::RCMeshFitting() {

	myID = id++;
	//Initialise cmgui and get context
	std::stringstream cname;
	cname << "singlefit" << myID;
	context_ = Cmiss_context_create(cname.str().c_str()); /**< Handle to the context */
	//This required for using the cmiss_context_execute_command
	Cmiss_context_enable_user_interface(context_, NULL);

	Cmiss_region_id root_region = Cmiss_context_get_default_region(context_);

	//Load the finite element mesh of the heart

	// Read in the Hermite elements
	Cmiss_stream_information_id estream_information = Cmiss_region_create_stream_information(root_region);
	Cmiss_stream_resource_id estream_resource = 0;

	estream_resource = Cmiss_stream_information_create_resource_memory_buffer(estream_information, refinedrcheart_exregion, refinedrcheart_exregion_len);

	Cmiss_region_read(root_region, estream_information);

	Cmiss_stream_resource_destroy(&estream_resource);
	Cmiss_stream_information_destroy(&estream_information);

	field_module_ = Cmiss_region_get_field_module(root_region);
	if (field_module_ != 0) {
		Cmiss_field_module_begin_change(field_module_);

		// 'coordinates' is an assumed field from the element template file.  It is assumed to be
		// rc coordinate system.

		coordinates_rc_ = Cmiss_field_module_find_field_by_name(field_module_, "coordinates");

		if (!coordinates_rc_) {
			std::cerr << " Unable to obtain RC coordinates field, fatal! Quiting" << std::endl;
			throw -1;
		}

		cache = Cmiss_field_module_create_cache(field_module_);
		// Cannot make all these fields via the Cmgui API yet.
		Cmiss_field_id ds1_ = Cmiss_field_module_create_field(field_module_, "d_ds1", "node_value fe_field coordinates d/ds1");
		Cmiss_field_id ds2_ = Cmiss_field_module_create_field(field_module_, "d_ds2", "node_value fe_field coordinates d/ds2");
		Cmiss_field_id ds1ds2_ = Cmiss_field_module_create_field(field_module_, "d2_ds1ds2", "node_value fe_field coordinates d2/ds1ds2");

		Cmiss_field_destroy(&ds1_);
		Cmiss_field_destroy(&ds2_);
		Cmiss_field_destroy(&ds1ds2_);

		Cmiss_field_module_end_change(field_module_);

		Cmiss_nodeset_id nodeset = Cmiss_field_module_find_nodeset_by_name(field_module_, "cmiss_nodes");
		Cmiss_node_iterator_id nodeIterator = Cmiss_nodeset_create_node_iterator(nodeset);

		Cmiss_node_id node = Cmiss_node_iterator_next(nodeIterator);
		meshNodeID.resize(98);
		if (node != 0) {
			while (node) {
				int node_id = Cmiss_node_get_identifier(node);
				int idx = node_id - 1;
				meshNodeID[idx] = node;
				node = Cmiss_node_iterator_next(nodeIterator);
			}
		}

		Cmiss_nodeset_destroy(&nodeset);
		Cmiss_node_iterator_destroy(&nodeIterator);

	}

	Cmiss_region_destroy(&root_region);
}

RCMeshFitting::~RCMeshFitting() {
	Cmiss_field_destroy(&coordinates_rc_);
	if (field_module_ != 0) {
		for (int i = 0; i < 98; i++) {
			Cmiss_node_destroy(&meshNodeID[i]);
		}
		Cmiss_field_cache_destroy(&cache);
		Cmiss_field_module_destroy(&field_module_);
	}
	Cmiss_context_destroy(&context_);
}

std::string RCMeshFitting::getOptimizedMesh(std::vector<Point3D>& markers, std::vector<Point3D>* emarkers) {
//Since cmgui outputs all fields by default create another instance and update the coordinates
	const unsigned int nodeOrder[] = { 57, 55, 53, 52, 63, 61, 60, 69, 67, 66, 73, 71, 56, 54, 51, 50, 62, 59, 58, 68, 65, 64, 72, 70, 77, 76, 75, 74, 80, 79, 78, 83, 82, 81, 85,
			84, 89, 88, 87, 86, 92, 91, 90, 95, 94, 93, 97, 96, 98, 8, 6, 4, 3, 14, 12, 11, 20, 18, 17, 24, 22, 7, 5, 2, 1, 13, 10, 9, 19, 16, 15, 23, 21, 28, 27, 26, 25, 31, 30,
			29, 34, 33, 32, 36, 35, 40, 39, 38, 37, 43, 42, 41, 46, 45, 44, 48, 47, 49 };
	std::vector<Point3D> jpc(98); //98 nodes
	Point3D init;
	unsigned int numMarkers = markers.size();
//Set the input coordinates
	Cmiss_field_module_begin_change(field_module_);
	double temp_array[3];
	for (int i = 0; i < numMarkers; i++) {
		int nodeID = nodeOrder[i] - 1;
		jpc[nodeID] = markers[i];
		temp_array[0] = jpc[nodeID].x;
		temp_array[1] = jpc[nodeID].y;
		temp_array[2] = jpc[nodeID].z;
		Cmiss_field_cache_set_node(cache, meshNodeID[nodeID]);
		Cmiss_field_assign_real(coordinates_rc_, cache, 3, temp_array);
	}

	if (emarkers != NULL) {
		Cmiss_region_id root_region = Cmiss_context_get_default_region(context_);
		Cmiss_region_id marker_region = Cmiss_region_create_child(root_region, "Marker");
		Cmiss_field_module_id field_module = Cmiss_region_get_field_module(marker_region);
		Cmiss_region_destroy(&marker_region);
		Cmiss_field_module_begin_change(field_module);

		Cmiss_field_id composite = Cmiss_field_module_create_field(field_module, "coordinates", "finite number_of_components 3 component_names x y z");
		Cmiss_nodeset_id nodeset = Cmiss_field_module_find_nodeset_by_name(field_module, "cmiss_nodes");
		Cmiss_node_template_id nodeTemplate = Cmiss_nodeset_create_node_template(nodeset);

		Cmiss_node_template_define_field(nodeTemplate, composite);
		Cmiss_field_cache_id cachex = Cmiss_field_module_create_cache(field_module);

		//The input coordinates are in rectangular Cartesian coordinates
		//Just the endo nodes
		for (int i = 0; i < emarkers->size(); i++) {
			Cmiss_node_id temporaryNode = Cmiss_nodeset_create_node(nodeset, 101 + i /*Fix this */, nodeTemplate);
			Cmiss_field_cache_set_node(cachex, temporaryNode);

			double loc[3];
			Point3D& pt = emarkers->at(i);
			loc[0] = pt.x;
			loc[1] = pt.y;
			loc[2] = pt.z;

			Cmiss_field_assign_real(composite, cachex, 3, loc);
			Cmiss_node_destroy(&temporaryNode);
		}
		Cmiss_field_module_end_change(field_module);

		Cmiss_node_template_destroy(&nodeTemplate);
		Cmiss_nodeset_destroy(&nodeset);
		Cmiss_field_destroy(&composite);
		Cmiss_field_cache_destroy(&cachex);
		Cmiss_field_module_destroy(&field_module);
	}

	Cmiss_context_execute_command(context_, "gfx smooth field coordinates");
//This unfortunately screws up the apex nodes.. the values for all versions are not set
//Set the derivatives to 0 as other values seem to lead the apex to have a canal
	double ds1[3], ds2[3], ds3[3], ds1ds2[3], ds2ds3[3], ds1ds3[3], ds1ds2ds3[3];
	ds1[0] = ds1[1] = ds1[2] = ds2[0] = ds2[1] = ds2[2] = ds3[0] = ds3[1] = ds3[2] = 0.0;
	ds1ds2[0] = ds1ds2[1] = ds1ds2[2] = ds1ds3[0] = ds1ds3[1] = ds1ds3[2] = 0.0;
	ds2ds3[0] = ds2ds3[1] = ds2ds3[2] = ds1ds2ds3[0] = ds1ds2ds3[1] = ds1ds2ds3[2] = 0.0;

	double epids1[3], epids2[3], epids3[3], epids1ds2[3], epids2ds3[3], epids1ds3[3], epids1ds2ds3[3];
	epids1[0] = epids1[1] = epids1[2] = epids2[0] = epids2[1] = epids2[2] = epids3[0] = epids3[1] = epids3[2] = 0.0;
	epids1ds2[0] = epids1ds2[1] = epids1ds2[2] = epids1ds3[0] = epids1ds3[1] = epids1ds3[2] = 0.0;
	epids2ds3[0] = epids2ds3[1] = epids2ds3[2] = epids1ds2ds3[0] = epids1ds2ds3[1] = epids1ds2ds3[2] = 0.0;

	for (int v = 1; v < 13; v++) {
		//Create a node value field with version nos
		std::ostringstream ss;
		ss << "coordinates_" << v;
		std::string cname = ss.str();
		ss.str("");
		ss << "node_value fe_field coordinates version " << v;
		std::string command = ss.str();
		ss.str("");
		ss << "" << v;
		std::string version = ss.str();
		Cmiss_field_id cv = Cmiss_field_module_find_field_by_name(field_module_, cname.c_str());
		if (!cv)
			cv = Cmiss_field_module_create_field(field_module_, cname.c_str(), command.c_str());
		Cmiss_field_id ds1_ = Cmiss_field_module_find_field_by_name(field_module_, ("d_ds1" + version).c_str());
		if (!ds1_)
			ds1_ = Cmiss_field_module_create_field(field_module_, ("d_ds1" + version).c_str(), ("node_value fe_field coordinates d/ds1 version " + version).c_str());
		Cmiss_field_id ds2_ = Cmiss_field_module_find_field_by_name(field_module_, ("d_ds2" + version).c_str());
		if (!ds2_)
			ds2_ = Cmiss_field_module_create_field(field_module_, ("d_ds2" + version).c_str(), ("node_value fe_field coordinates d/ds2 version " + version).c_str());
		Cmiss_field_id ds3_ = Cmiss_field_module_find_field_by_name(field_module_, ("d_ds3" + version).c_str());
		if (!ds3_)
			ds3_ = Cmiss_field_module_create_field(field_module_, ("d_ds3" + version).c_str(), ("node_value fe_field coordinates d/ds3 version " + version).c_str());
		Cmiss_field_id ds1ds2_ = Cmiss_field_module_find_field_by_name(field_module_, ("d_ds1ds2" + version).c_str());
		if (!ds1ds2_)
			ds1ds2_ = Cmiss_field_module_create_field(field_module_, ("d_ds1ds2" + version).c_str(), ("node_value fe_field coordinates d2/ds1ds2 version " + version).c_str());
		Cmiss_field_id ds1ds3_ = Cmiss_field_module_find_field_by_name(field_module_, ("d_ds1ds3" + version).c_str());
		if (!ds1ds3_)
			ds1ds3_ = Cmiss_field_module_create_field(field_module_, ("d_ds1ds3" + version).c_str(), ("node_value fe_field coordinates d2/ds1ds3 version " + version).c_str());
		Cmiss_field_id ds2ds3_ = Cmiss_field_module_find_field_by_name(field_module_, ("d2_ds2ds3" + version).c_str());
		if (!ds2ds3_)
			ds2ds3_ = Cmiss_field_module_create_field(field_module_, ("d2_ds2ds3" + version).c_str(), ("node_value fe_field coordinates d2/ds2ds3 version " + version).c_str());
		Cmiss_field_id ds1ds2ds3_ = Cmiss_field_module_find_field_by_name(field_module_, ("d3_ds1ds2ds3" + version).c_str());
		if (!ds1ds2ds3_)
			ds1ds2ds3_ = Cmiss_field_module_create_field(field_module_, ("d3_ds1ds2ds3" + version).c_str(),
					("node_value fe_field coordinates d3/ds1ds2ds3 version " + version).c_str());

		Cmiss_field_cache_set_node(cache, meshNodeID[48]);
		temp_array[0] = jpc[48].x;
		temp_array[1] = jpc[48].y;
		temp_array[2] = jpc[48].z;
		Cmiss_field_assign_real(cv, cache, 3, temp_array);
		Cmiss_field_assign_real(ds1_, cache, 3, ds1);
		Cmiss_field_assign_real(ds2_, cache, 3, ds2);
		Cmiss_field_assign_real(ds3_, cache, 3, ds3);
		Cmiss_field_assign_real(ds1ds2_, cache, 3, ds1ds2);
		Cmiss_field_assign_real(ds1ds3_, cache, 3, ds1ds3);
		Cmiss_field_assign_real(ds2ds3_, cache, 3, ds2ds3);
		Cmiss_field_assign_real(ds1ds2ds3_, cache, 3, ds1ds2ds3);

		Cmiss_field_cache_set_node(cache, meshNodeID[97]);
		temp_array[0] = jpc[97].x;
		temp_array[1] = jpc[97].y;
		temp_array[2] = jpc[97].z;
		Cmiss_field_assign_real(cv, cache, 3, temp_array);
		Cmiss_field_assign_real(ds1_, cache, 3, epids1);
		Cmiss_field_assign_real(ds2_, cache, 3, epids2);
		Cmiss_field_assign_real(ds3_, cache, 3, epids3);
		Cmiss_field_assign_real(ds1ds2_, cache, 3, epids1ds2);
		Cmiss_field_assign_real(ds1ds3_, cache, 3, epids1ds3);
		Cmiss_field_assign_real(ds2ds3_, cache, 3, epids2ds3);
		Cmiss_field_assign_real(ds1ds2ds3_, cache, 3, epids1ds2ds3);

		Cmiss_field_destroy(&cv);
		Cmiss_field_destroy(&ds1_);
		Cmiss_field_destroy(&ds2_);
		Cmiss_field_destroy(&ds1ds2_);
		Cmiss_field_destroy(&ds1ds3_);
		Cmiss_field_destroy(&ds3_);
		Cmiss_field_destroy(&ds2ds3_);
		Cmiss_field_destroy(&ds1ds3_);
		Cmiss_field_destroy(&ds1ds2ds3_);
	}

	Cmiss_field_module_end_change(field_module_);

	Cmiss_region_id root_region = Cmiss_context_get_default_region(context_);
	Cmiss_stream_information_id stream_information = Cmiss_region_create_stream_information(root_region);
	Cmiss_stream_information_region_id region_stream_information = Cmiss_stream_information_cast_region(stream_information);
	Cmiss_stream_resource_id stream = Cmiss_stream_information_create_resource_memory(stream_information);
	Cmiss_region_write(root_region, stream_information);
	void *memory_buffer = NULL;
	unsigned int memory_buffer_size = 0;
	Cmiss_stream_resource_memory_id memory_resource = Cmiss_stream_resource_cast_memory(stream);
	Cmiss_stream_resource_memory_get_buffer_copy(memory_resource, &memory_buffer, &memory_buffer_size);
	std::string resultx;
	if (memory_buffer && memory_buffer_size) {
		std::string temp((char*) memory_buffer, memory_buffer_size);
		resultx = temp;
	}
	Cmiss_stream_resource_memory_destroy(&memory_resource);
	Cmiss_stream_resource_destroy(&(stream));

	free(memory_buffer);

	Cmiss_stream_information_destroy(&stream_information);
	Cmiss_stream_information_region_destroy(&region_stream_information);
	Cmiss_region_destroy(&root_region);

	return resultx;

}
