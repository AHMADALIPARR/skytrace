(* SPDX-License-Identifier: AGPL-3.0-only OR LicenseRef-SnapKitty-Opaque-1.0 *)
(* Copyright (C) 2026 Ahmad Ali Parr / SnapKitty Research Lab *)
(* NDJSON port: each incoming line is either a record or {"records":[...]}; each
   outgoing line is {"ok":true,"records":[...]} or a structured failure. *)
open Yojson.Basic.Util
let member_opt name obj = try member name obj with _ -> `Null
let str name obj = match member_opt name obj with `String x when String.trim x <> "" -> x | _ -> failwith ("required_string:" ^ name)
let opt_str name obj = match member_opt name obj with `String x when String.trim x <> "" -> Some x | _ -> None
let flag name obj = match member_opt name obj with `Bool true -> `String "present" | `Bool false -> `String "absent" | `Null -> `String "unknown" | _ -> failwith ("invalid_bool:" ^ name)
let valid_mime s = String.contains s '/' && not (String.contains s '\n') && not (String.contains s '\r')
let normalize obj =
  let agency = str "agency" obj and canonical_id = str "canonical_id" obj and release_epoch = str "release_epoch" obj and mime = str "mime" obj in
  if not (valid_mime mime) then failwith "invalid_mime";
  let source = str "source" obj in
  if not (List.mem source ["pursue";"nara";"aaro"]) then failwith "invalid_source";
  let parent_id = opt_str "parent_id" obj in
  let resolved = flag "resolved" obj in
  let has_range = flag "has_range_telemetry" obj and corroborated = flag "corroborated" obj in
  let active = resolved <> `String "present" && (has_range <> `String "present" || corroborated <> `String "present") in
  let key = agency ^ "\x1f" ^ canonical_id ^ "\x1f" ^ release_epoch ^ "\x1f" ^ mime in
  (key,parent_id,`Assoc ["source",`String source;"agency",`String agency;"canonical_id",`String canonical_id;
    "release_epoch",`String release_epoch;"mime",`String mime;"parent_id",(match parent_id with None->`Null|Some x->`String x);
    "has_range_telemetry",has_range;"corroborated",corroborated;"resolved",resolved;"active",`Bool active;
    "key",`String key])
let validate_forest rows =
  let keys = Hashtbl.create (List.length rows) and ids = Hashtbl.create (List.length rows) in
  List.iter (fun (key,_,obj) ->
    if Hashtbl.mem keys key then failwith "duplicate_composite_key";
    Hashtbl.add keys key ();
    let id = str "canonical_id" obj in
    if Hashtbl.mem ids id then failwith "ambiguous_parent_id";
    Hashtbl.add ids id obj) rows;
  List.iter (fun (_,parent,obj) ->
    let visited = Hashtbl.create 8 in
    let rec walk = function
      | None -> ()
      | Some p ->
        if Hashtbl.mem visited p then failwith "containment_cycle";
        Hashtbl.add visited p ();
        match Hashtbl.find_opt ids p with
        | None -> failwith "unknown_parent_id"
        | Some next -> walk (opt_str "parent_id" next)
    in
    Hashtbl.add visited (str "canonical_id" obj) ();
    walk parent) rows
let process json =
  let records = match member_opt "records" json with `List xs -> xs | `Null -> [json] | _ -> failwith "records_not_array" in
  let rows = List.map normalize records in
  validate_forest rows;
  `Assoc ["ok",`Bool true;"records",`List (List.map (fun (_,_,obj)->obj) rows)]
let () =
  (try while true do
    let line = read_line () in
    let reply = try process (Yojson.Basic.from_string line) with
      | Yojson.Json_error s -> `Assoc ["ok",`Bool false;"error",`String ("json:" ^ s)]
      | Failure s -> `Assoc ["ok",`Bool false;"error",`String s]
      | Type_error (s,_) -> `Assoc ["ok",`Bool false;"error",`String ("type:" ^ s)] in
    print_endline (Yojson.Basic.to_string reply)
  done with End_of_file -> ())
