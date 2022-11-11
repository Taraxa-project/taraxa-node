/*
@licstart  The following is the entire license notice for the
JavaScript code in this file.

Copyright (C) 1997-2019 by Dimitri van Heesch

This program is free software; you can redistribute it and/or modify
it under the terms of version 2 of the GNU General Public License as published by
the Free Software Foundation

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.

@licend  The above is the entire license notice
for the JavaScript code in this file
*/
var NAVTREE =
[
  [ "TARAXA", "index.html", [
    [ "Introducing Taraxa", "index.html", [
      [ "Whitepaper", "index.html#autotoc_md1", null ],
      [ "Quickstart", "index.html#autotoc_md2", null ],
      [ "Downloading", "index.html#autotoc_md3", null ],
      [ "Building", "index.html#autotoc_md6", null ],
      [ "Running", "index.html#autotoc_md7", null ],
      [ "Contributing", "index.html#autotoc_md10", null ],
      [ "Useful doc", "index.html#autotoc_md11", null ],
      [ "System Requirements", "index.html#autotoc_md12", null ]
    ] ],
    [ "Building taraxa-node", "md_doc_building.html", [
      [ "Building on Ubuntu 22.04", "md_doc_building.html#autotoc_md27", [
        [ "Compile-Time Options (cmake)", "md_doc_building.html#autotoc_md25", [
          [ "Docker image", "index.html#autotoc_md4", null ],
          [ "Ubuntu binary", "index.html#autotoc_md5", null ],
          [ "Inside docker image", "index.html#autotoc_md8", null ],
          [ "Pre-built binary or manual build:", "index.html#autotoc_md9", null ],
          [ "CMAKE_BUILD_TYPE=[Release/Debug/RelWithDebInfo]", "md_doc_building.html#autotoc_md26", null ]
        ] ],
        [ "Install taraxa-node dependencies:", "md_doc_building.html#autotoc_md28", null ],
        [ "Clone the Repository", "md_doc_building.html#autotoc_md29", null ],
        [ "Compile", "md_doc_building.html#autotoc_md30", null ]
      ] ],
      [ "Building on Ubuntu 20.04", "md_doc_building.html#autotoc_md31", [
        [ "Install taraxa-node dependencies:", "md_doc_building.html#autotoc_md32", null ],
        [ "Clone the Repository", "md_doc_building.html#autotoc_md33", null ],
        [ "Compile", "md_doc_building.html#autotoc_md34", null ]
      ] ],
      [ "Building on MacOS", "md_doc_building.html#autotoc_md35", [
        [ "Install taraxa-node dependencies:", "md_doc_building.html#autotoc_md36", null ],
        [ "Clone the Repository", "md_doc_building.html#autotoc_md37", null ],
        [ "Compile", "md_doc_building.html#autotoc_md38", null ],
        [ "Known issues", "md_doc_building.html#autotoc_md39", [
          [ "Issues with conan cache", "md_doc_building.html#autotoc_md40", null ],
          [ "Project building issue", "md_doc_building.html#autotoc_md41", null ]
        ] ]
      ] ],
      [ "Building on M1 Macs for x86_64 with Rosetta2", "md_doc_building.html#autotoc_md42", [
        [ "Install Rosetta2", "md_doc_building.html#autotoc_md43", null ],
        [ "Run an x86_64 session", "md_doc_building.html#autotoc_md44", null ],
        [ "Install Homebrew", "md_doc_building.html#autotoc_md45", null ],
        [ "Install dependencies", "md_doc_building.html#autotoc_md46", null ],
        [ "Clone the Repository", "md_doc_building.html#autotoc_md47", null ],
        [ "Compile", "md_doc_building.html#autotoc_md48", null ]
      ] ],
      [ "Run", "md_doc_building.html#autotoc_md49", [
        [ "Running tests", "md_doc_building.html#autotoc_md50", null ],
        [ "Running taraxa-node", "md_doc_building.html#autotoc_md51", null ]
      ] ],
      [ "Building using \"taraxa-builder\" docker image", "md_doc_building.html#autotoc_md52", null ]
    ] ],
    [ "C++ Best Practices Guidelines", "md_doc_coding_practices.html", [
      [ "The structure of this document", "md_doc_coding_practices.html#autotoc_md54", [
        [ "<a name=\"compiletimechecking\"></a> 1. Prefer compile-time checking to run-time checking", "md_doc_coding_practices.html#autotoc_md55", null ],
        [ "<a name=\"avoidnonconst\"></a> 2. Avoid non-<tt>const</tt> global variables", "md_doc_coding_practices.html#autotoc_md59", null ],
        [ "<a name=\"ruleofzero\"></a> 3. If you can avoid defining default operations, do", "md_doc_coding_practices.html#autotoc_md66", null ],
        [ "<a name=\"ruleoffive\"></a> 4. If you define or <tt>=delete</tt> any default operation, define or <tt>=delete</tt> them all", "md_doc_coding_practices.html#autotoc_md71", null ],
        [ "<a name=\"smartptr\"></a> 5. Never transfer ownership by a raw pointer (<tt>T*</tt>) or reference (<tt>T&</tt>)", "md_doc_coding_practices.html#autotoc_md80", null ],
        [ "<a name=\"usingsmartptr\"></a> 6. Choose appropriate smart pointer or why we have more smart pointers?", "md_doc_coding_practices.html#autotoc_md85", null ],
        [ "<a name=\"singlealloc\"></a> 7. Perform at most one explicit resource allocation in a single expression statement", "md_doc_coding_practices.html#autotoc_md89", null ],
        [ "<a name=\"passsmartptr\"></a> 8. Take smart pointers as parameters only to explicitly express lifetime semantics", "md_doc_coding_practices.html#autotoc_md93", null ],
        [ "<a name=\"varlimitscope\"></a> 9. Declare names in for-statement initializers and conditions to limit scope", "md_doc_coding_practices.html#autotoc_md98", null ],
        [ "<a name=\"varrecycle\"></a> 10. Don't use a variable for two unrelated purposes", "md_doc_coding_practices.html#autotoc_md104", null ],
        [ "<a name=\"macrosconsts\"></a> 11. Don't use macros for constants or \"functions\"", "md_doc_coding_practices.html#autotoc_md109", null ],
        [ "<a name=\"magicconstants\"></a> 12. Avoid \"magic constants\"; use symbolic constants", "md_doc_coding_practices.html#autotoc_md113", null ],
        [ "<a name=\"nullptr\"></a> 13. Use <tt>nullptr</tt> rather than <tt>0</tt> or <tt>NULL</tt>", "md_doc_coding_practices.html#autotoc_md117", null ],
        [ "<a name=\"construct\"></a> 14. Use the <tt>T{e}</tt>notation for construction", "md_doc_coding_practices.html#autotoc_md121", null ],
        [ "<a name=\"referencecapture\"></a> 15. Prefer capturing by reference in lambdas that will be used locally, including passed to algorithms", "md_doc_coding_practices.html#autotoc_md127", null ],
        [ "<a name=\"valuecapture\"></a> 16. Avoid capturing by reference in lambdas that will be used nonlocally, including returned, stored on the heap, or passed to another thread", "md_doc_coding_practices.html#autotoc_md134", null ],
        [ "<a name=\"thiscapture\"></a> 17. If you capture <tt>this</tt>, capture all variables explicitly (no default capture)", "md_doc_coding_practices.html#autotoc_md139", null ],
        [ "<a name=\"defaultctor\"></a> 18. Don't define a default constructor that only initializes data members; use in-class member initializers instead", "md_doc_coding_practices.html#autotoc_md144", null ],
        [ "<a name=\"explicitctor\"></a> 19. By default, declare single-argument constructors explicit", "md_doc_coding_practices.html#autotoc_md149", null ],
        [ "<a name=\"alwaysinitialize\"></a> 20. Always initialize an object", "md_doc_coding_practices.html#autotoc_md154", null ],
        [ "<a name=\"lambdainit\"></a> 21. Use lambdas for complex initialization, especially of <tt>const</tt> variables", "md_doc_coding_practices.html#autotoc_md165", null ],
        [ "<a name=\"orderctor\"></a> 22. Define and initialize member variables in the order of member declaration", "md_doc_coding_practices.html#autotoc_md171", null ],
        [ "<a name=\"inclassinitializer\"></a> 23. Prefer in-class initializers to member initializers in constructors for constant initializers", "md_doc_coding_practices.html#autotoc_md175", null ],
        [ "<a name=\"initializetoassignment\"></a> 24. Prefer initialization to assignment in constructors", "md_doc_coding_practices.html#autotoc_md180", null ],
        [ "<a name=\"castsnamed\"></a> 25. If you must use a cast, use a named cast", "md_doc_coding_practices.html#autotoc_md184", null ],
        [ "<a name=\"castsconst\"></a> 26. Don't cast away <tt>const</tt>", "md_doc_coding_practices.html#autotoc_md190", null ],
        [ "<a name=\"symmetric\"></a> 27. Use nonmember functions for symmetric operators", "md_doc_coding_practices.html#autotoc_md197", null ]
      ] ]
    ] ],
    [ "Contributing Guide", "md_doc_contributing.html", null ],
    [ "Doxygen", "md_doc_doxygen.html", [
      [ "Installing dependencies", "md_doc_doxygen.html#autotoc_md203", [
        [ "Ubuntu", "md_doc_doxygen.html#autotoc_md204", null ],
        [ "MacOS", "md_doc_doxygen.html#autotoc_md205", null ]
      ] ],
      [ "Generating documentation", "md_doc_doxygen.html#autotoc_md206", null ],
      [ "Commenting", "md_doc_doxygen.html#autotoc_md207", [
        [ "Basic example", "md_doc_doxygen.html#autotoc_md208", null ],
        [ "Grouping to modules", "md_doc_doxygen.html#autotoc_md209", null ],
        [ "Graphs", "md_doc_doxygen.html#autotoc_md210", null ]
      ] ]
    ] ],
    [ "Git-flow Guide", "md_doc_git_practices.html", [
      [ "Branch naming conventions", "md_doc_git_practices.html#autotoc_md211", null ],
      [ "Main branches", "md_doc_git_practices.html#autotoc_md212", null ],
      [ "Supporting branches", "md_doc_git_practices.html#autotoc_md213", [
        [ "Standard Feature branches", "md_doc_git_practices.html#autotoc_md214", null ],
        [ "Long-term Feature branches", "md_doc_git_practices.html#autotoc_md215", null ],
        [ "Hotfix branches", "md_doc_git_practices.html#autotoc_md216", null ],
        [ "Release branches", "md_doc_git_practices.html#autotoc_md217", null ]
      ] ],
      [ "Branches Cleaning", "md_doc_git_practices.html#autotoc_md218", null ],
      [ "PR merging & Code reviews", "md_doc_git_practices.html#autotoc_md219", null ],
      [ "Commit message conventions", "md_doc_git_practices.html#autotoc_md220", null ],
      [ "Automatic github issues linking", "md_doc_git_practices.html#autotoc_md224", null ],
      [ "Example", "md_doc_git_practices.html#autotoc_md225", null ]
    ] ],
    [ "Quickstart Guide", "md_doc_quickstart_guide.html", [
      [ "Taraxa docker image", "md_doc_quickstart_guide.html#autotoc_md234", [
        [ "Pre-requisites", "md_doc_quickstart_guide.html#autotoc_md227", [
          [ "\"type\" must be one of the following mentioned below!", "md_doc_git_practices.html#autotoc_md221", null ],
          [ "\"scope\" is optional", "md_doc_git_practices.html#autotoc_md222", null ],
          [ "\"subject\"", "md_doc_git_practices.html#autotoc_md223", null ],
          [ "Used git flow:", "md_doc_git_practices.html#autotoc_md226", null ],
          [ "MANDATORY PORT", "md_doc_quickstart_guide.html#autotoc_md228", null ],
          [ "OPTIONAL PORTS", "md_doc_quickstart_guide.html#autotoc_md229", null ]
        ] ],
        [ "Config", "md_doc_quickstart_guide.html#autotoc_md230", [
          [ "Param1", "md_doc_quickstart_guide.html#autotoc_md231", null ],
          [ "Param2", "md_doc_quickstart_guide.html#autotoc_md232", null ],
          [ "...", "md_doc_quickstart_guide.html#autotoc_md233", null ],
          [ "taraxa-builder:latest", "md_doc_quickstart_guide.html#autotoc_md235", null ],
          [ "taraxa-node:latest", "md_doc_quickstart_guide.html#autotoc_md236", null ]
        ] ]
      ] ]
    ] ],
    [ "Standard release cycle", "md_doc_release_cycle.html", [
      [ "Release cycle phases", "md_doc_release_cycle.html#autotoc_md238", [
        [ "Phase 1 - active development of new features", "md_doc_release_cycle.html#autotoc_md239", null ],
        [ "Phase 2 - alpha testing (internal)", "md_doc_release_cycle.html#autotoc_md240", null ],
        [ "Phase 3 - beta testing (public)", "md_doc_release_cycle.html#autotoc_md241", null ],
        [ "Phase 4 - Mainnet release", "md_doc_release_cycle.html#autotoc_md242", null ]
      ] ],
      [ "Ad-hoc releases with bug fixes", "md_doc_release_cycle.html#autotoc_md243", null ]
    ] ],
    [ "Rewards distribution algorithm", "md_doc_rewards_distribution.html", [
      [ "Glossary", "md_doc_rewards_distribution.html#autotoc_md245", null ],
      [ "Rewards sources", "md_doc_rewards_distribution.html#autotoc_md246", null ],
      [ "Rewards distribution", "md_doc_rewards_distribution.html#autotoc_md247", [
        [ "Beneficial work in network", "md_doc_rewards_distribution.html#autotoc_md248", null ],
        [ "Newly created tokens:", "md_doc_rewards_distribution.html#autotoc_md249", null ],
        [ "Included transactions fees:", "md_doc_rewards_distribution.html#autotoc_md250", null ]
      ] ],
      [ "Validators statistics", "md_doc_rewards_distribution.html#autotoc_md251", null ],
      [ "Example:", "md_doc_rewards_distribution.html#autotoc_md252", [
        [ "DAG structure:", "md_doc_rewards_distribution.html#autotoc_md253", null ],
        [ "PBFT block", "md_doc_rewards_distribution.html#autotoc_md254", null ],
        [ "Statistics", "md_doc_rewards_distribution.html#autotoc_md255", null ],
        [ "Rewards", "md_doc_rewards_distribution.html#autotoc_md256", [
          [ "DAG blocks rewards", "md_doc_rewards_distribution.html#autotoc_md257", null ],
          [ "PBFT proposer reward", "md_doc_rewards_distribution.html#autotoc_md258", null ],
          [ "PBFT voters reward", "md_doc_rewards_distribution.html#autotoc_md259", null ]
        ] ]
      ] ]
    ] ],
    [ "Taraxa RPC", "md_doc__r_p_c.html", [
      [ "Ethereum compatibility", "md_doc__r_p_c.html#autotoc_md261", [
        [ "Quirks", "md_doc__r_p_c.html#autotoc_md262", null ],
        [ "Not implemented", "md_doc__r_p_c.html#autotoc_md263", null ]
      ] ],
      [ "Taraxa specific methods", "md_doc__r_p_c.html#autotoc_md264", [
        [ "taraxa_protocolVersion", "md_doc__r_p_c.html#autotoc_md265", [
          [ "Parameters", "md_doc__r_p_c.html#autotoc_md266", null ],
          [ "Returns", "md_doc__r_p_c.html#autotoc_md267", null ],
          [ "Example", "md_doc__r_p_c.html#autotoc_md268", null ]
        ] ],
        [ "taraxa_getVersion", "md_doc__r_p_c.html#autotoc_md269", [
          [ "Parameters", "md_doc__r_p_c.html#autotoc_md270", null ],
          [ "Returns", "md_doc__r_p_c.html#autotoc_md271", null ],
          [ "Example", "md_doc__r_p_c.html#autotoc_md272", null ]
        ] ],
        [ "taraxa_getDagBlockByHash", "md_doc__r_p_c.html#autotoc_md273", [
          [ "Parameters", "md_doc__r_p_c.html#autotoc_md274", null ],
          [ "Returns", "md_doc__r_p_c.html#autotoc_md275", null ],
          [ "Example", "md_doc__r_p_c.html#autotoc_md276", null ]
        ] ],
        [ "taraxa_getDagBlockByLevel", "md_doc__r_p_c.html#autotoc_md277", [
          [ "Parameters", "md_doc__r_p_c.html#autotoc_md278", null ],
          [ "Returns", "md_doc__r_p_c.html#autotoc_md279", null ],
          [ "Example", "md_doc__r_p_c.html#autotoc_md280", null ]
        ] ],
        [ "taraxa_dagBlockLevel", "md_doc__r_p_c.html#autotoc_md281", [
          [ "Parameters", "md_doc__r_p_c.html#autotoc_md282", null ],
          [ "Returns", "md_doc__r_p_c.html#autotoc_md283", null ],
          [ "Example", "md_doc__r_p_c.html#autotoc_md284", null ]
        ] ],
        [ "taraxa_dagBlockPeriod", "md_doc__r_p_c.html#autotoc_md285", [
          [ "Parameters", "md_doc__r_p_c.html#autotoc_md286", null ],
          [ "Returns", "md_doc__r_p_c.html#autotoc_md287", null ],
          [ "Example", "md_doc__r_p_c.html#autotoc_md288", null ]
        ] ],
        [ "taraxa_getScheduleBlockByPeriod", "md_doc__r_p_c.html#autotoc_md289", [
          [ "Parameters", "md_doc__r_p_c.html#autotoc_md290", null ],
          [ "Returns", "md_doc__r_p_c.html#autotoc_md291", null ],
          [ "Example", "md_doc__r_p_c.html#autotoc_md292", null ]
        ] ],
        [ "taraxa_getConfig", "md_doc__r_p_c.html#autotoc_md293", [
          [ "Parameters", "md_doc__r_p_c.html#autotoc_md294", null ],
          [ "Returns", "md_doc__r_p_c.html#autotoc_md295", null ],
          [ "Example", "md_doc__r_p_c.html#autotoc_md296", null ]
        ] ]
      ] ],
      [ "Test API", "md_doc__r_p_c.html#autotoc_md297", [
        [ "get_sortition_change", "md_doc__r_p_c.html#autotoc_md298", [
          [ "Parameters", "md_doc__r_p_c.html#autotoc_md299", null ],
          [ "Returns", "md_doc__r_p_c.html#autotoc_md300", null ],
          [ "Example", "md_doc__r_p_c.html#autotoc_md301", null ]
        ] ],
        [ "send_coin_transaction", "md_doc__r_p_c.html#autotoc_md302", [
          [ "Parameters", "md_doc__r_p_c.html#autotoc_md303", null ],
          [ "Returns", "md_doc__r_p_c.html#autotoc_md304", null ],
          [ "Example", "md_doc__r_p_c.html#autotoc_md305", null ]
        ] ],
        [ "send_coin_transactions", "md_doc__r_p_c.html#autotoc_md306", [
          [ "Parameters", "md_doc__r_p_c.html#autotoc_md307", null ],
          [ "Returns", "md_doc__r_p_c.html#autotoc_md308", null ]
        ] ],
        [ "get_account_address", "md_doc__r_p_c.html#autotoc_md309", [
          [ "Parameters", "md_doc__r_p_c.html#autotoc_md310", null ],
          [ "Returns", "md_doc__r_p_c.html#autotoc_md311", null ],
          [ "Example", "md_doc__r_p_c.html#autotoc_md312", null ]
        ] ],
        [ "get_peer_count", "md_doc__r_p_c.html#autotoc_md313", [
          [ "Parameters", "md_doc__r_p_c.html#autotoc_md314", null ],
          [ "Returns", "md_doc__r_p_c.html#autotoc_md315", null ],
          [ "Example", "md_doc__r_p_c.html#autotoc_md316", null ]
        ] ],
        [ "get_node_status", "md_doc__r_p_c.html#autotoc_md317", [
          [ "Parameters", "md_doc__r_p_c.html#autotoc_md318", null ],
          [ "Returns", "md_doc__r_p_c.html#autotoc_md319", null ],
          [ "Example", "md_doc__r_p_c.html#autotoc_md320", null ]
        ] ],
        [ "get_packets_stats", "md_doc__r_p_c.html#autotoc_md321", [
          [ "Parameters", "md_doc__r_p_c.html#autotoc_md322", null ],
          [ "Returns", "md_doc__r_p_c.html#autotoc_md323", null ],
          [ "Example", "md_doc__r_p_c.html#autotoc_md324", null ]
        ] ],
        [ "get_all_nodes", "md_doc__r_p_c.html#autotoc_md325", [
          [ "Parameters", "md_doc__r_p_c.html#autotoc_md326", null ],
          [ "Returns", "md_doc__r_p_c.html#autotoc_md327", null ],
          [ "Example", "md_doc__r_p_c.html#autotoc_md328", null ]
        ] ]
      ] ]
    ] ],
    [ "readme", "md_doc_uml_readme.html", null ],
    [ "Todo List", "todo.html", null ],
    [ "Modules", "modules.html", "modules" ],
    [ "Namespace Members", "namespacemembers.html", [
      [ "All", "namespacemembers.html", "namespacemembers_dup" ],
      [ "Functions", "namespacemembers_func.html", "namespacemembers_func" ],
      [ "Variables", "namespacemembers_vars.html", null ],
      [ "Typedefs", "namespacemembers_type.html", null ],
      [ "Enumerations", "namespacemembers_enum.html", null ],
      [ "Enumerator", "namespacemembers_eval.html", null ]
    ] ],
    [ "Classes", "annotated.html", [
      [ "Class List", "annotated.html", "annotated_dup" ],
      [ "Class Index", "classes.html", null ],
      [ "Class Hierarchy", "hierarchy.html", "hierarchy" ],
      [ "Class Members", "functions.html", [
        [ "All", "functions.html", "functions_dup" ],
        [ "Functions", "functions_func.html", "functions_func" ],
        [ "Variables", "functions_vars.html", "functions_vars" ],
        [ "Typedefs", "functions_type.html", null ],
        [ "Enumerations", "functions_enum.html", null ],
        [ "Enumerator", "functions_eval.html", null ],
        [ "Related Functions", "functions_rela.html", null ]
      ] ]
    ] ],
    [ "Files", "files.html", [
      [ "File List", "files.html", "files_dup" ],
      [ "File Members", "globals.html", [
        [ "All", "globals.html", null ],
        [ "Functions", "globals_func.html", null ],
        [ "Variables", "globals_vars.html", null ],
        [ "Typedefs", "globals_type.html", null ],
        [ "Macros", "globals_defs.html", null ]
      ] ]
    ] ],
    [ "Examples", "examples.html", "examples" ]
  ] ]
];

var NAVTREEINDEX =
[
"_a_e_s_8cpp.html",
"_log_8h_source.html",
"class_server_interface.html#aa2a8f065c3e92b7050ba6e31064e965a",
"classdev_1_1_r_l_p_stream.html#acc18c2677d6f60e8b11658ffd969ba0d",
"classdev_1_1p2p_1_1_node_table.html#ae6be78eae27ea37219e69e649699431b",
"classgraphql_1_1taraxa_1_1_dag_block.html#a77ba05ccdedf027940bf5a26e821637b",
"classtaraxa_1_1_db_storage.html#aaf414edf032dcd3b006c80a7aa86a042",
"classtaraxa_1_1_two_t_plus_one_soft_voted_block_data.html#a23c16bf09f99e29d4525f351be880faa",
"classtaraxa_1_1net_1_1_eth_face.html#aadaca0b66e3b9600804d12654b4aa2f4",
"classtaraxa_1_1net_1_1_ws_session.html#af54dde7e17a487cd1505576fd4e6e90b",
"classtaraxa_1_1network_1_1tarcap_1_1_packet_data.html#a72226f0737859bf3bce92f03b375e8ef",
"classtaraxa_1_1network_1_1tarcap_1_1_tarcap_thread_pool.html#a856ab885ac6cc4e16e8e83701badcb7d",
"dir_09afb69cf3f50004f558d1b5bd58d008.html",
"get__votes__sync__packet__handler_8hpp_source.html",
"group___final_chain.html#a266e83f473963f7ab585279f6e50d835",
"group___p_b_f_t.html#a4d979bf979a9b432496cac771bb13049",
"group___vote.html#a2078e19b96d7ca2d0a651d38ed69de2a",
"libdevcore_2_exceptions_8h.html#a0fce2e8d2a40506fc15e2ce75792e580",
"md_doc_building.html#autotoc_md29",
"namespacedev_1_1p2p.html#a4c2c3d4a753e9c972c46049cd3c2205e",
"pbft__manager_8hpp.html#ggaa671924d435517347832c7f40f1f6cfcaac67fcdf2515b0a2fcc7747d14b52ce3",
"structdev_1_1p2p_1_1_e_n_r_request.html#a8ef3ec3f78e539f49c51c0a2f5cfa06a",
"structdev_1_1p2p_1_1_r_l_p_x_handshake.html#a34c98fc05d267e74ef1bdbfe6b1e36d9",
"structtaraxa_1_1_transaction.html#a2f1197d218527ec86a7265df9b77e2f2",
"tools_8hpp.html#ad950c77c2b9ca495ef9166b3514f537f"
];

var SYNCONMSG = 'click to disable panel synchronisation';
var SYNCOFFMSG = 'click to enable panel synchronisation';