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
    [ "Taraxa RPC", "md_doc__r_p_c.html", [
      [ "Ethereum compatibility", "md_doc__r_p_c.html#autotoc_md245", [
        [ "Quirks", "md_doc__r_p_c.html#autotoc_md246", null ],
        [ "Not implemented", "md_doc__r_p_c.html#autotoc_md247", null ]
      ] ],
      [ "Taraxa specific methods", "md_doc__r_p_c.html#autotoc_md248", [
        [ "taraxa_protocolVersion", "md_doc__r_p_c.html#autotoc_md249", [
          [ "Parameters", "md_doc__r_p_c.html#autotoc_md250", null ],
          [ "Returns", "md_doc__r_p_c.html#autotoc_md251", null ],
          [ "Example", "md_doc__r_p_c.html#autotoc_md252", null ]
        ] ],
        [ "taraxa_getVersion", "md_doc__r_p_c.html#autotoc_md253", [
          [ "Parameters", "md_doc__r_p_c.html#autotoc_md254", null ],
          [ "Returns", "md_doc__r_p_c.html#autotoc_md255", null ],
          [ "Example", "md_doc__r_p_c.html#autotoc_md256", null ]
        ] ],
        [ "taraxa_getDagBlockByHash", "md_doc__r_p_c.html#autotoc_md257", [
          [ "Parameters", "md_doc__r_p_c.html#autotoc_md258", null ],
          [ "Returns", "md_doc__r_p_c.html#autotoc_md259", null ],
          [ "Example", "md_doc__r_p_c.html#autotoc_md260", null ]
        ] ],
        [ "taraxa_getDagBlockByLevel", "md_doc__r_p_c.html#autotoc_md261", [
          [ "Parameters", "md_doc__r_p_c.html#autotoc_md262", null ],
          [ "Returns", "md_doc__r_p_c.html#autotoc_md263", null ],
          [ "Example", "md_doc__r_p_c.html#autotoc_md264", null ]
        ] ],
        [ "taraxa_dagBlockLevel", "md_doc__r_p_c.html#autotoc_md265", [
          [ "Parameters", "md_doc__r_p_c.html#autotoc_md266", null ],
          [ "Returns", "md_doc__r_p_c.html#autotoc_md267", null ],
          [ "Example", "md_doc__r_p_c.html#autotoc_md268", null ]
        ] ],
        [ "taraxa_dagBlockPeriod", "md_doc__r_p_c.html#autotoc_md269", [
          [ "Parameters", "md_doc__r_p_c.html#autotoc_md270", null ],
          [ "Returns", "md_doc__r_p_c.html#autotoc_md271", null ],
          [ "Example", "md_doc__r_p_c.html#autotoc_md272", null ]
        ] ],
        [ "taraxa_getScheduleBlockByPeriod", "md_doc__r_p_c.html#autotoc_md273", [
          [ "Parameters", "md_doc__r_p_c.html#autotoc_md274", null ],
          [ "Returns", "md_doc__r_p_c.html#autotoc_md275", null ],
          [ "Example", "md_doc__r_p_c.html#autotoc_md276", null ]
        ] ],
        [ "taraxa_getConfig", "md_doc__r_p_c.html#autotoc_md277", [
          [ "Parameters", "md_doc__r_p_c.html#autotoc_md278", null ],
          [ "Returns", "md_doc__r_p_c.html#autotoc_md279", null ],
          [ "Example", "md_doc__r_p_c.html#autotoc_md280", null ]
        ] ]
      ] ],
      [ "Test API", "md_doc__r_p_c.html#autotoc_md281", [
        [ "get_sortition_change", "md_doc__r_p_c.html#autotoc_md282", [
          [ "Parameters", "md_doc__r_p_c.html#autotoc_md283", null ],
          [ "Returns", "md_doc__r_p_c.html#autotoc_md284", null ],
          [ "Example", "md_doc__r_p_c.html#autotoc_md285", null ]
        ] ],
        [ "send_coin_transaction", "md_doc__r_p_c.html#autotoc_md286", [
          [ "Parameters", "md_doc__r_p_c.html#autotoc_md287", null ],
          [ "Returns", "md_doc__r_p_c.html#autotoc_md288", null ],
          [ "Example", "md_doc__r_p_c.html#autotoc_md289", null ]
        ] ],
        [ "get_account_address", "md_doc__r_p_c.html#autotoc_md290", [
          [ "Parameters", "md_doc__r_p_c.html#autotoc_md291", null ],
          [ "Returns", "md_doc__r_p_c.html#autotoc_md292", null ],
          [ "Example", "md_doc__r_p_c.html#autotoc_md293", null ]
        ] ],
        [ "get_peer_count", "md_doc__r_p_c.html#autotoc_md294", [
          [ "Parameters", "md_doc__r_p_c.html#autotoc_md295", null ],
          [ "Returns", "md_doc__r_p_c.html#autotoc_md296", null ],
          [ "Example", "md_doc__r_p_c.html#autotoc_md297", null ]
        ] ],
        [ "get_node_status", "md_doc__r_p_c.html#autotoc_md298", [
          [ "Parameters", "md_doc__r_p_c.html#autotoc_md299", null ],
          [ "Returns", "md_doc__r_p_c.html#autotoc_md300", null ],
          [ "Example", "md_doc__r_p_c.html#autotoc_md301", null ]
        ] ],
        [ "get_packets_stats", "md_doc__r_p_c.html#autotoc_md302", [
          [ "Parameters", "md_doc__r_p_c.html#autotoc_md303", null ],
          [ "Returns", "md_doc__r_p_c.html#autotoc_md304", null ],
          [ "Example", "md_doc__r_p_c.html#autotoc_md305", null ]
        ] ],
        [ "get_all_nodes", "md_doc__r_p_c.html#autotoc_md306", [
          [ "Parameters", "md_doc__r_p_c.html#autotoc_md307", null ],
          [ "Returns", "md_doc__r_p_c.html#autotoc_md308", null ],
          [ "Example", "md_doc__r_p_c.html#autotoc_md309", null ]
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
"class_server_interface.html#a9fd42772552cf2c0b31b7dd4b054286f",
"classdev_1_1_r_l_p_stream.html#ac9dd1fbf28a69e1d5e26e998b468bc37",
"classdev_1_1p2p_1_1_node_table.html#ae1d9a153f6d72f9fd7915952d55ec69f",
"classgraphql_1_1taraxa_1_1_dag_block.html#a62db2b51be5f3c8941927e095d7b9076",
"classtaraxa_1_1_db_storage.html#ab2d590a0858867c0ee145834b81f15c8",
"classtaraxa_1_1_two_t_plus_one_soft_voted_block_data.html#a129eef2a43a256ad2cdf7202886c2425",
"classtaraxa_1_1net_1_1_eth_face.html#aadaca0b66e3b9600804d12654b4aa2f4",
"classtaraxa_1_1net_1_1rpc_1_1eth_1_1_eth_impl.html#a05051bdf84e5df1d7a4f6a9b920db275",
"classtaraxa_1_1network_1_1tarcap_1_1_packet_data.html#a9a6b3c17f0fc132cf03cb89d3baa112aab4c37a041c3901d84ec46dda683c6fd8",
"classtaraxa_1_1network_1_1tarcap_1_1_test_state.html#a16540d04373bf8e0176d4f14554b852d",
"dir_1e80da6770b7154a4f5e1b5fe61458cc.html",
"graphql_2include_2graphql_2ws__server_8hpp.html",
"group___final_chain.html#a30e8d0c8a0085d31299ba486459f670d",
"group___p_b_f_t.html#a5c07848027fb2b186ddb512e9e5b9cd8",
"group___vote.html#a2f609a3b7a3ea1f5adb0e026abad8246",
"libdevcore_2_exceptions_8h.html#a4b4b10242be59dd68c1749205272e2ec",
"md_doc_building.html#autotoc_md43",
"namespacemembers_func_g.html",
"range__view_8hpp.html#adcd4894881eed9624e39b04dd4009e64",
"structdev_1_1p2p_1_1_host.html#a25003fb227d136001b5986c6f1bc35bd",
"structdev_1_1p2p_1_1_r_l_p_x_handshake.html#aa8c61c0b068f04afce351af0c1d6bb77",
"structtaraxa_1_1_transaction_1_1_invalid_signature.html#a7974a0937e6fff51c0ff13f9e57b238f",
"types_2transaction_2include_2transaction_2transaction_8hpp.html"
];

var SYNCONMSG = 'click to disable panel synchronisation';
var SYNCOFFMSG = 'click to enable panel synchronisation';