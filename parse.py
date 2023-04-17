import os
import itertools
import sys
import pprint
import re
import ctypes
import xml.etree.ElementTree as ET
# path = "DAI\\Reference\\xml"

def hash(text):
    h = ctypes.c_uint32(5381)
    for l in text:
        h = ctypes.c_uint32((h.value * 33) ^ ord(l))
    return h.value

def hhash(text):
    s = format(hash(text), "#010x")
    return s

files = [os.path.join(dp, f) for dp, dn, filenames in os.walk(path) for f in filenames]

tags = set()
values = set()
ids = set()

for f in files:
    with open(f, encoding='utf8') as file:
        it = itertools.chain('<root>', file, '</root>')
        try:
            root = ET.fromstringlist(it)
        except Exception as e:
            print(e)
            print(f)
        nodes = root.iter()
        for n in nodes:
            tags.add(str(n.tag))
            if 'Id' in n.tag or 'field' in n.tag or 'Field' in n.tag or 'ID' in n.tag:
                ids.add(str(n.text))
            else:
                values.add(str(n.text))


def isNumber(s):
    return re.search(r'[^\d.\-+_E\s]', s) == None

def cutGUID(s):
    return re.sub(re.compile(r'\s*[a-f0-9]{8}-[a-f0-9]{4}-[a-f0-9]{4}-[a-f0-9]{4}-[a-f0-9]{12}\s*', re.IGNORECASE), '', s)

def isHex(s):
    return re.fullmatch(re.compile(r'[a-fA-F0-9]*[0-9]+[a-fA-F0-9]*|[A-F]{4}'), s)

def isHash(s):
    res = re.fullmatch(re.compile(r'0x[a-f0-9]{8}', re.IGNORECASE), s)
    if res != None:
        if len(res.group(0)) != 10:
            print(s)
        return True
    return False

def isSpace(s):
    return re.fullmatch(r'[\n\s]+', s) != None

def onlyHashes(s):
    return {x for x in s if isHash(x)}

def filterSet(s):
    return {cutGUID(x) for x in s if not isNumber(x) and not isHash(x) and not isHex(x) and not isSpace(x)}

def replaceChars(s):
    return s.replace("&amp;", "&").replace("^", ">").replace("∨", "<")
    
def splitStr(s):
    return re.findall(r'(?:[A-DF-Z]|E(?!\+|-))(?:[a-z]+[0-9]*|[A-Z]*[0-9]*(?![a-z])|$)|[a-z]+[0-9]*|[0-9]+[^\w\/\s\n\]\[(),.]+[0-9]+|[^\w\/\s\n\]\[\-(),."%&$\'!:{}]+|[0-9]+[a-zA-DF-Z]+(?!\+|-)', replaceChars(s))


tagHashes = onlyHashes(tags)
valueHashes = onlyHashes(values)
idHashes = onlyHashes(ids)

filteredTags = filterSet(tags)
filteredValues = filterSet(values)
filteredIds = filterSet(ids)

tagParts = {}
valueParts = {}
idParts = {}

for t in filteredTags:
    for p in splitStr(t):
        if p in tagParts:
            tagParts[p] += 1
        else:
            tagParts[p] = 1

for v in filteredValues:
    for p in splitStr(v):
        if p in valueParts:
            valueParts[p] += 1
        else:
            valueParts[p] = 1

for v in filteredIds:
    for p in splitStr(v):
        if p in idParts:
            idParts[p] += 1
        else:
            idParts[p] = 1

hashedText = {}

with open('meta.txt', 'w') as metaFile:
    print(f"Unique tags: {len(tags)}", file=metaFile)
    print(f"Unique values: {len(values)}", file=metaFile)
    print(f"Unique id values: {len(ids)}\n", file=metaFile)
    print(f"Tag Hashes: {len(tagHashes)}", file=metaFile)
    print(f"Value Hashes: {len(valueHashes)}", file=metaFile)
    print(f"Id Hashes: {len(idHashes)}\n", file=metaFile)
    print(f"Filtered tags: {len(filteredTags)}", file=metaFile)
    print(f"Filtered values: {len(filteredValues)}", file=metaFile)
    print(f"Filtered ids: {len(filteredIds)}\n", file=metaFile)
    print(f"Tag parts: {len(tagParts)}", file=metaFile)
    print(f"Value parts: {len(valueParts)}", file=metaFile)
    print(f"Ids parts: {len(idParts)}\n", file=metaFile)
    print(f"Tags that show up as values: {len(filteredTags.intersection(filteredValues))}", file=metaFile)
    print(f"Tags that show up as ids: {len(filteredTags.intersection(filteredIds))}\n", file=metaFile)


    with open("values.txt", 'w', encoding='utf8') as valuesFile:
        for k, e in sorted(sorted(valueParts.items()), key=lambda item: item[1], reverse=True):
            h = hhash(k)
            if h not in hashedText:
                hashedText[h] = k
            elif (hashedText[h] != k):
                print(f"Duplicate hashes for: {hashedText[h]} | {k}")
            print(f"{k.ljust(60)}{str(e).ljust(10)}{h}", file=valuesFile)
    with open("tags.txt", 'w', encoding='utf8') as tagsFile:
        for k, t in sorted(sorted(tagParts.items()), key=lambda item: item[1], reverse=True):
            h = hhash(k)
            if h not in hashedText:
                hashedText[h] = k
            elif (hashedText[h] != k):
                print(f"Duplicate hashes for: {hashedText[h]} | {k}")
            print(f"{k.ljust(60)}{str(t).ljust(10)}{h}", file=tagsFile)
    with open("ids.txt", 'w', encoding='utf8') as idsFile:
        for k, e in sorted(sorted(idParts.items()), key=lambda item: item[1], reverse=True):
            h = hhash(k)
            if h not in hashedText:
                hashedText[h] = k
            elif (hashedText[h] != k):
                print(f"Duplicate hashes for: {hashedText[h]} | {k}")
            print(f"{k.ljust(60)}{str(e).ljust(10)}{h}", file=idsFile)
    with open("fullTags.txt", 'w', encoding='utf8') as fullTagsFile:
        for t in sorted(filteredTags):
            h = hhash(t)
            if h not in hashedText:
                hashedText[h] = t
            elif (hashedText[h] != t):
                print(f"Duplicate hashes for: {hashedText[h]} | {t}")
            print(f"{t.ljust(100)}{h}", file=fullTagsFile)
    with open("fullIds.txt", 'w', encoding='utf8') as fullIdsFile:
        for t in sorted(filteredIds):
            h = hhash(t)
            if h not in hashedText:
                hashedText[h] = t
            elif (hashedText[h] != t):
                print(f"Duplicate hashes for: {hashedText[h]} | {t}")
            print(f"{t.ljust(100)}{h}", file=fullIdsFile)

    allHashes = valueHashes.union(idHashes).union(tagHashes)
    unusedTagHashes = set(hashedText.keys()).difference(allHashes)
    unTranslatedHashes = set(allHashes.difference(hashedText.keys()))

    with open('untranslatedHashes.txt', 'w', encoding="utf8") as unTranslatedFile:
        for u in unTranslatedHashes:
          print(f"{u}", file=unTranslatedFile)
    
    with open('unusedHashes.txt', 'w', encoding="utf8") as unusedHashesFile:
        for u in unusedTagHashes:
          print(f"{hashedText[u].ljust(100)}{u}", file=unusedHashesFile)
    
    
    print(f"Unused Tags and hashes: {len(unusedTagHashes)}", file=metaFile)
    print(f"Untranslated hashes used elsewhere as values: {len(unTranslatedHashes)}", file=metaFile)
    


# # separate on: spaces, underscores, camel case; keep symbols between letters together; numbers + letters stay together

# # & = &amp;
# # > = ^
# # < = ∨