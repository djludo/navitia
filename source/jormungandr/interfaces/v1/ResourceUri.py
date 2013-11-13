from flask.ext.restful import Resource
from converters_collection_type import collections_to_resource_type, resource_type_to_collection
from make_links import add_id_links, clean_links, add_pagination_links
from functools import wraps
from collections import OrderedDict
from flask import url_for
from flask.ext.restful.utils import unpack
from authentification import authentification_required
import type_pb2

class ResourceUri(Resource):
    def __init__(self, *args, **kwargs):
        Resource.__init__(self, *args, **kwargs)
        self.region = None
        self.method_decorators = []
        self.method_decorators.append(add_id_links())
        self.method_decorators.append(add_address_poi_id(self))
        self.method_decorators.append(add_computed_resources(self))
        self.method_decorators.append(add_pagination_links())
        self.method_decorators.append(clean_links())
        self.method_decorators.append(authentification_required)


    def get_filter(self, items):
        filters = []
        if len(items) % 2 != 0:
            items = items[:-1]
        type_ = None

        for item in items:
            if not type_:
                if item != "coord":
                    type_ = collections_to_resource_type[item]
                else:
                    type_ = "coord"
            else:
                if type_ == "coord" or type_ == "address":
                    splitted_coord = item.split(";")
                    if len(splitted_coord) == 2:
                        lon, lat = splitted_coord
                        object_type = "stop_point"
                        if(self.collection == "pois"):
                            object_type = "poi"
                        filters.append(object_type+".coord DWITHIN("+lon+","+lat+",200)")
                    else:
                        filters.append(type_+".uri="+item)
                elif type_ == 'poi':
                    filters.append(type_+'.uri='+item.split(":")[-1])
                else :
                    filters.append(type_+".uri="+item)
                type_ = None
        return " and ".join(filters)

class add_computed_resources(object):
    def __init__(self, resource):
        self.resource = resource
    def __call__(self, f):
        @wraps(f)
        def wrapper(*args, **kwargs):
            response = f(*args, **kwargs)
            if isinstance(response, tuple):
                data, code, header = unpack(response)
            else:
                data = response
            collection = None
            kwargs["_external"] = True
            templated = True
            for key in data.keys():
                if key in collections_to_resource_type.keys():
                    collection = key
                if key in resource_type_to_collection.keys():
                    collection = resource_type_to_collection[key]
            if collection is None:
                return response
            kwargs["uri"] = collection + '/'
            if "id" in kwargs.keys():
                kwargs["uri"] += kwargs["id"]
                del kwargs["id"]
                templated = False
            else:
                kwargs["uri"] += '{' + collection + ".id}"
            if collection in ['stop_areas', 'stop_points', 'lines', 'routes', 'addresses']:
                for api in ['route_schedules', 'stop_schedules',
                            'arrivals', 'departures', "places_nearby"]:
                    data['links'].append({
                        "href" : url_for("v1."+api, **kwargs),
                        "rel" : api,
                        "templated" : templated
                            })
            if collection in ['stop_areas', 'stop_points', 'addresses']:
                data['links'].append({
                    "href" : url_for("v1.journeys", **kwargs),
                    "rel" : "journeys",
                    "templated" : templated
                    })
            if isinstance(response, tuple):
                return data, code, header
            else:
                return data
        return wrapper


class add_address_poi_id(object):
    def __init__(self, resource):
        self.resource = resource

    def __call__(self, f):
        @wraps(f)
        def wrapper(*args, **kwargs):
            objects = f(*args, **kwargs)
            def add_id(objects, region, type_ = None):
                if isinstance(objects, list) or isinstance(objects, tuple):
                    for item in objects:
                        add_id(item, region, type_)
                elif isinstance(objects, dict) or\
                     isinstance(objects, OrderedDict):
                         for v in objects.keys():
                             add_id(objects[v], region, v)
                         if 'address' == type_:
                            lon = objects['coord']['lon']
                            lat = objects['coord']['lat']
                            objects['id'] = lon +';'+ lat
                         if type_ == 'poi' or type_ == 'pois' :
                            old_id = objects['id']
                            objects['id'] = 'poi:'+region+':'+ old_id
                         if type_ == 'administrative_region' or\
                            type_ == 'administrative_regions':
                             old_id = objects['id']
                             objects['id'] = 'admin:'+region+old_id[5:]
                         if 'embedded_type' in objects.keys() and\
                                (objects['embedded_type'] == 'address'  or\
                                  objects['embedded_type'] == 'poi' or\
                                  objects['embedded_type'] == 'administrative_region'):
                            objects["id"] = objects[objects['embedded_type']]["id"]
            if self.resource.region:
                add_id(objects, self.resource.region)
            return objects
        return wrapper


class add_notes(object):
    def __init__(self, resource):
        self.resource = resource

    def __call__(self, f):
        @wraps(f)
        def wrapper(*args, **kwargs):
            objects = f(*args, **kwargs)
            if isinstance(objects, tuple):
                data, code, header = unpack(objects)
            else:
                data = objects

            def add_note(data):
                result = []
                if isinstance(data, list) or isinstance(data, tuple):
                    for item in data:
                        result.extend(add_note(item))

                elif isinstance(data, dict) or\
                     isinstance(data, OrderedDict):
                         if 'type' in data.keys() and data['type'] == 'notes':
                            result.append({"id" : data['id'], "value" :  data['value']})
                            del data["value"]
                         else :
                             for v in data.items():
                                 result.extend(add_note(v))
                return result
            if self.resource.region:
                if not "notes" in data.keys() or  not isinstance(data["notes"], list):
                    data["notes"] = []
                    result = []
                    result_note = add_note(data)
                    [result.append(item) for item in result_note if not item in result]
                    data["notes"].extend(result)

            if isinstance(objects, tuple):
                return data, code, header
            else:
                return data

        return wrapper

class update_journeys_status(object):
    def __init__(self, resource):
        self.resource = resource

    def __call__(self, f):
        @wraps(f)
        def wrapper(*args, **kwargs):
            objects = f(*args, **kwargs)
            if isinstance(objects, tuple):
                data, code, header = unpack(objects)
            else:
                data = objects

            def update_status(journey, _items):

                if isinstance(_items, list) or isinstance(_items, tuple):
                    for item in _items:
                        update_status(journey, item)
                elif isinstance(_items, dict) or\
                         isinstance(_items, OrderedDict):
                         if 'messages' in _items.keys():
                            for msg in _items["messages"]:
                                if not "status" in journey.keys():
                                    journey["status"] = msg["level"]
                                else:
                                    message_status = type_pb2.Message.DESCRIPTOR.fields_by_name['message_status'].enum_type.values_by_name
                                    if message_status[journey["status"]] < message_status[msg["level"]]:
                                        journey["status"] = msg["level"]
                         else:
                             for v in _items.items():
                                 update_status(journey, v)

            if self.resource.region and data.has_key("journeys"):
               for journey in data["journeys"]:
                    update_status(journey, journey)

            if isinstance(objects, tuple):
                return data, code, header
            else:
                return data

        return wrapper

class manage_response_status(object):
    def __init__(self, resource):
        self.resource = resource

    def __call__(self, f):
        @wraps(f)
        def wrapper(*args, **kwargs):
            objects = f(*args, **kwargs)
            if isinstance(objects, tuple):
                data, code, header = unpack(objects)
            else:
                data = objects

            def response_status(data):
                if 'stop_schedules' in data.keys():
                    stop_schedules = data["stop_schedules"]
                    for one_stop_schedule in stop_schedules:
                        if 'date_times' in one_stop_schedule.keys():
                            if len(one_stop_schedule["date_times"]) == 0:
                                one_stop_schedule["status"] = "no_departure_this_date"
                        else:
                            one_stop_schedule["status"] = "no_departure_this_date"

            if self.resource.region:
                    response_status(data)
            if isinstance(objects, tuple):
                return data, code, header
            else:
                return data

        return wrapper