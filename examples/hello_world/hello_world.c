#include "exb/exb.h"
#include "exb/http/http_server_module.h"
#include "exb/http/http_request.h"

struct hello_world_module {
    struct exb_http_server_module head;
    struct exb *exb_ref;
    int count;
};
static int handle_request(struct exb_http_server_module *module, struct exb_request_state *rqstate, int reason) {
    struct hello_world_module *mod = (struct hello_world_module *) module;

    struct exb_str path;
    //exb_request_repr(rqstate);

    exb_str_init_empty(&path);
    exb_str_slice_to_copied_str(mod->exb_ref, rqstate->path_s, rqstate->input_buffer, &path);
    if (exb_str_streqc(mod->exb_ref, &path, "/big")) {
        struct exb_str key,value;
        exb_str_init_const_str(&key, "Content-Type");
        exb_str_init_const_str(&value, "text/html");
        exb_response_append_body_cstr(rqstate, 
"0\r\n1\r\n2\r\n3\r\n4\r\n5\r\n6\r\n7\r\n8\r\n9\r\n10\r\n11\r\n"
"12\r\n13\r\n14\r\n15\r\n16\r\n17\r\n18\r\n19\r\n20\r\n21\r\n22\r\n23\r\n"
"24\r\n25\r\n26\r\n27\r\n28\r\n29\r\n30\r\n31\r\n32\r\n33\r\n34\r\n35\r\n"
"36\r\n37\r\n38\r\n39\r\n40\r\n41\r\n42\r\n43\r\n44\r\n45\r\n46\r\n47\r\n"
"48\r\n49\r\n50\r\n51\r\n52\r\n53\r\n54\r\n55\r\n56\r\n57\r\n58\r\n59\r\n"
"60\r\n61\r\n62\r\n63\r\n64\r\n65\r\n66\r\n67\r\n68\r\n69\r\n70\r\n71\r\n"
"72\r\n73\r\n74\r\n75\r\n76\r\n77\r\n78\r\n79\r\n80\r\n81\r\n82\r\n83\r\n"
"84\r\n85\r\n86\r\n87\r\n88\r\n89\r\n90\r\n91\r\n92\r\n93\r\n94\r\n95\r\n"
"96\r\n97\r\n98\r\n99\r\n100\r\n101\r\n102\r\n103\r\n104\r\n105\r\n106\r\n107\r\n"
"108\r\n109\r\n110\r\n111\r\n112\r\n113\r\n114\r\n115\r\n116\r\n117\r\n118\r\n119\r\n"
"120\r\n121\r\n122\r\n123\r\n124\r\n125\r\n126\r\n127\r\n128\r\n129\r\n130\r\n131\r\n"
"132\r\n133\r\n134\r\n135\r\n136\r\n137\r\n138\r\n139\r\n140\r\n141\r\n142\r\n143\r\n"
"144\r\n145\r\n146\r\n147\r\n148\r\n149\r\n150\r\n151\r\n152\r\n153\r\n154\r\n155\r\n"
"156\r\n157\r\n158\r\n159\r\n160\r\n161\r\n162\r\n163\r\n164\r\n165\r\n166\r\n167\r\n"
"168\r\n169\r\n170\r\n171\r\n172\r\n173\r\n174\r\n175\r\n176\r\n177\r\n178\r\n179\r\n"
"180\r\n181\r\n182\r\n183\r\n184\r\n185\r\n186\r\n187\r\n188\r\n189\r\n190\r\n191\r\n"
"192\r\n193\r\n194\r\n195\r\n196\r\n197\r\n198\r\n199\r\n200\r\n201\r\n202\r\n203\r\n"
"204\r\n205\r\n206\r\n207\r\n208\r\n209\r\n210\r\n211\r\n212\r\n213\r\n214\r\n215\r\n"
"216\r\n217\r\n218\r\n219\r\n220\r\n221\r\n222\r\n223\r\n224\r\n225\r\n226\r\n227\r\n"
"228\r\n229\r\n230\r\n231\r\n232\r\n233\r\n234\r\n235\r\n236\r\n237\r\n238\r\n239\r\n"
"240\r\n241\r\n242\r\n243\r\n244\r\n245\r\n246\r\n247\r\n248\r\n249\r\n250\r\n251\r\n"
"252\r\n253\r\n254\r\n255\r\n256\r\n257\r\n258\r\n259\r\n260\r\n261\r\n262\r\n263\r\n"
"264\r\n265\r\n266\r\n267\r\n268\r\n269\r\n270\r\n271\r\n272\r\n273\r\n274\r\n275\r\n"
"276\r\n277\r\n278\r\n279\r\n280\r\n281\r\n282\r\n283\r\n284\r\n285\r\n286\r\n287\r\n"
"288\r\n289\r\n290\r\n291\r\n292\r\n293\r\n294\r\n295\r\n296\r\n297\r\n298\r\n299\r\n"
"300\r\n301\r\n302\r\n303\r\n304\r\n305\r\n306\r\n307\r\n308\r\n309\r\n310\r\n311\r\n"
"312\r\n313\r\n314\r\n315\r\n316\r\n317\r\n318\r\n319\r\n320\r\n321\r\n322\r\n323\r\n"
"324\r\n325\r\n326\r\n327\r\n328\r\n329\r\n330\r\n331\r\n332\r\n333\r\n334\r\n335\r\n"
"336\r\n337\r\n338\r\n339\r\n340\r\n341\r\n342\r\n343\r\n344\r\n345\r\n346\r\n347\r\n"
"348\r\n349\r\n350\r\n351\r\n352\r\n353\r\n354\r\n355\r\n356\r\n357\r\n358\r\n359\r\n"
"360\r\n361\r\n362\r\n363\r\n364\r\n365\r\n366\r\n367\r\n368\r\n369\r\n370\r\n371\r\n"
"372\r\n373\r\n374\r\n375\r\n376\r\n377\r\n378\r\n379\r\n380\r\n381\r\n382\r\n383\r\n"
"384\r\n385\r\n386\r\n387\r\n388\r\n389\r\n390\r\n391\r\n392\r\n393\r\n394\r\n395\r\n"
"396\r\n397\r\n398\r\n399\r\n400\r\n401\r\n402\r\n403\r\n404\r\n405\r\n406\r\n407\r\n"
"408\r\n409\r\n410\r\n411\r\n412\r\n413\r\n414\r\n415\r\n416\r\n417\r\n418\r\n419\r\n"
"420\r\n421\r\n422\r\n423\r\n424\r\n425\r\n426\r\n427\r\n428\r\n429\r\n430\r\n431\r\n"
"432\r\n433\r\n434\r\n435\r\n436\r\n437\r\n438\r\n439\r\n440\r\n441\r\n442\r\n443\r\n"
"444\r\n445\r\n446\r\n447\r\n448\r\n449\r\n450\r\n451\r\n452\r\n453\r\n454\r\n455\r\n"
"456\r\n457\r\n458\r\n459\r\n460\r\n461\r\n462\r\n463\r\n464\r\n465\r\n466\r\n467\r\n"
"468\r\n469\r\n470\r\n471\r\n472\r\n473\r\n474\r\n475\r\n476\r\n477\r\n478\r\n479\r\n"
"480\r\n481\r\n482\r\n483\r\n484\r\n485\r\n486\r\n487\r\n488\r\n489\r\n490\r\n491\r\n"
"492\r\n493\r\n494\r\n495\r\n496\r\n497\r\n498\r\n499\r\n500\r\n501\r\n502\r\n503\r\n"
"504\r\n505\r\n506\r\n507\r\n508\r\n509\r\n510\r\n511\r\n512\r\n513\r\n514\r\n515\r\n"
"516\r\n517\r\n518\r\n519\r\n520\r\n521\r\n522\r\n523\r\n524\r\n525\r\n526\r\n527\r\n"
"528\r\n529\r\n530\r\n531\r\n532\r\n533\r\n534\r\n535\r\n536\r\n537\r\n538\r\n539\r\n"
"540\r\n541\r\n542\r\n543\r\n544\r\n545\r\n546\r\n547\r\n548\r\n549\r\n550\r\n551\r\n"
"552\r\n553\r\n554\r\n555\r\n556\r\n557\r\n558\r\n559\r\n560\r\n561\r\n562\r\n563\r\n"
"564\r\n565\r\n566\r\n567\r\n568\r\n569\r\n570\r\n571\r\n572\r\n573\r\n574\r\n575\r\n"
"576\r\n577\r\n578\r\n579\r\n580\r\n581\r\n582\r\n583\r\n584\r\n585\r\n586\r\n587\r\n"
"588\r\n589\r\n590\r\n591\r\n592\r\n593\r\n594\r\n595\r\n596\r\n597\r\n598\r\n599\r\n"
"600\r\n601\r\n602\r\n603\r\n604\r\n605\r\n606\r\n607\r\n608\r\n609\r\n610\r\n611\r\n"
"612\r\n613\r\n614\r\n615\r\n616\r\n617\r\n618\r\n619\r\n620\r\n621\r\n622\r\n623\r\n"
"624\r\n625\r\n626\r\n627\r\n628\r\n629\r\n630\r\n631\r\n632\r\n633\r\n634\r\n635\r\n"
"636\r\n637\r\n638\r\n639\r\n640\r\n641\r\n642\r\n643\r\n644\r\n645\r\n646\r\n647\r\n"
"648\r\n649\r\n650\r\n651\r\n652\r\n653\r\n654\r\n655\r\n656\r\n657\r\n658\r\n659\r\n"
"660\r\n661\r\n662\r\n663\r\n664\r\n665\r\n666\r\n667\r\n668\r\n669\r\n670\r\n671\r\n"
"672\r\n673\r\n674\r\n675\r\n676\r\n677\r\n678\r\n679\r\n680\r\n681\r\n682\r\n683\r\n"
"684\r\n685\r\n686\r\n687\r\n688\r\n689\r\n690\r\n691\r\n692\r\n693\r\n694\r\n695\r\n"
"696\r\n697\r\n698\r\n699\r\n700\r\n701\r\n702\r\n703\r\n704\r\n705\r\n706\r\n707\r\n"
"708\r\n709\r\n710\r\n711\r\n712\r\n713\r\n714\r\n715\r\n716\r\n717\r\n718\r\n719\r\n"
"720\r\n721\r\n722\r\n723\r\n724\r\n725\r\n726\r\n727\r\n728\r\n729\r\n730\r\n731\r\n"
"732\r\n733\r\n734\r\n735\r\n736\r\n737\r\n738\r\n739\r\n740\r\n741\r\n742\r\n743\r\n"
"744\r\n745\r\n746\r\n747\r\n748\r\n749\r\n750\r\n751\r\n752\r\n753\r\n754\r\n755\r\n"
"756\r\n757\r\n758\r\n759\r\n760\r\n761\r\n762\r\n763\r\n764\r\n765\r\n766\r\n767\r\n"
"768\r\n769\r\n770\r\n771\r\n772\r\n773\r\n774\r\n775\r\n776\r\n777\r\n778\r\n779\r\n"
"780\r\n781\r\n782\r\n783\r\n784\r\n785\r\n786\r\n787\r\n788\r\n789\r\n790\r\n791\r\n"
"792\r\n793\r\n794\r\n795\r\n796\r\n797\r\n798\r\n"
);
        exb_response_end(rqstate);
    }
    else if (exb_str_streqc(mod->exb_ref, &path, "/post") || exb_str_streqc(mod->exb_ref, &path, "/post/")) {
        struct exb_str key,value;
        exb_str_init_const_str(&key, "Content-Type");
        exb_str_init_const_str(&value, "text/html");
        exb_response_append_body_cstr(rqstate, "<!DOCTYPE html>");
        exb_response_append_body_cstr(rqstate, "<html>"
                                                      "<head>"
                                                      "    <title>EXBIN!</title>"
                                                      "</head>"
                                                      "<body>");
        if (exb_request_has_body(rqstate)) {
            if (reason == EXB_HTTP_HANDLER_HEADERS) {
                rqstate->body_handling = EXB_HTTP_B_BUFFER;
            }
            else {
                exb_response_append_body_cstr(rqstate, "You posted: <br>");
                exb_response_append_body_cstr(rqstate, "<p>");

                /*XSS!*/
                if (rqstate->body_decoded.str)
                    exb_response_append_body_cstr(rqstate, rqstate->body_decoded.str);

                exb_response_append_body_cstr(rqstate, "</p>");
                exb_response_append_body_cstr(rqstate, "</body>"
                                                              "</html>");
                exb_response_end(rqstate);
            }
        }
        else {
            exb_response_set_header(rqstate, &key, &value);
            exb_response_append_body_cstr(rqstate, "<p> Post something! </p> <br>");
            exb_response_append_body_cstr(rqstate, "<form action=\"\" method=\"POST\">"
                                                          "Text: <input type=\"text\" name=\"text\"><br>"
                                                          "</form>");
            exb_response_append_body_cstr(rqstate, "</body>"
                                                          "</html>");
            exb_response_end(rqstate);
        }
        
    }
    else if (exb_str_startswithc(mod->exb_ref, &path, "/count")) 
    {
        struct exb_str key,value, tmp;
        exb_str_init_const_str(&key, "Content-Type");
        exb_str_init_const_str(&value, "text/html");
        exb_str_init_empty(&tmp);
        exb_sprintf(mod->exb_ref, &tmp, "<!DOCTYPE html>"
                                        "<html>"
                                        "<head>"
                                        "    <title>EXBIN!</title>"
                                        "</head>"
                                        "<body>"
                                        "<p>Count : <b>%d</b></p><br>"
                                        "<form action=\"/reset/\"><button>reset</button></form>"
                                        "</body>"
                                        "</html>", mod->count++);
        exb_response_append_body_cstr(rqstate, tmp.str);
        exb_str_deinit(mod->exb_ref, &tmp);
        exb_response_end(rqstate);
    }
    else if (exb_str_streqc(mod->exb_ref, &path, "/reset") || exb_str_startswithc(mod->exb_ref, &path, "/reset/")) {
        mod->count = 0;
        exb_response_redirect_and_end(rqstate, 307, "/count/");
    }
    else {
        struct exb_str key,value;
        exb_str_init_const_str(&key, "Content-Type");
        exb_str_init_const_str(&value, "text/plain");
        exb_response_set_header(rqstate, &key, &value);
        exb_response_append_body(rqstate, "Hello World!\r\n", 14);
        struct exb_str str;
        
        exb_str_init_empty(&str);
        
        exb_sprintf(mod->exb_ref, &str, "Requested URL: '%s'", path.str);
        
        exb_response_append_body(rqstate, str.str, str.len);
        exb_str_deinit(mod->exb_ref, &str);
        exb_response_end(rqstate);
    }
    
    exb_str_deinit(mod->exb_ref, &path);   
}
static void destroy_module(struct exb_http_server_module *module, struct exb *exb) {
    exb_free(exb, module);
}
int handler_init(struct exb *exb, struct exb_server *server, char *module_args, struct exb_http_server_module **module_out) {
    (void) module_args;
    struct hello_world_module *mod = exb_malloc(exb, sizeof(struct hello_world_module));
    if (!mod)
        return EXB_NOMEM_ERR;
    mod->exb_ref = exb;
    
    mod->head.destroy = destroy_module;
    mod->count = 0;
    
    if (exb_server_set_module_request_handler(server, (struct exb_http_server_module*)mod, handle_request) != EXB_OK) {
        destroy_module((struct exb_http_server_module*)mod, exb);
        return EXB_MODULE_LOAD_ERROR;
    }
    *module_out = (struct exb_http_server_module*)mod;
    return EXB_OK;
}
