import pymongo

huobi_db = pymongo.MongoClient('mongodb://localhost:27017/')['test']
kline_coll = huobi_db.get_collection('gk969')
data = {'id': 12323, "type": 'A'}
data['_id'] = data.pop('id')
kline_coll.insert_one(data)
