import random, os, sys, math
from PIL import Image
from PIL import ImageFilter
from PIL import ImageEnhance
from shutil import copyfile
import picar

import numpy as np
import matplotlib.mlab as mlab
import matplotlib.pyplot as plt

from cmath import rect, phase
from math import radians, degrees

from imgaug import augmenters as iaa

classMap = {"B": -1.0, "R": 0.45, "SR": 0.3, "F": 0.0, "SL": -0.3, "L": -0.45, "Bad": -2.0}
classNumber =  {"B": 0, "R": 1, "SR": 2, "F": 3, "SL": 4, "L": 5}

# Augmentation
imageStep = 13
center = [47, 13]
topCenter = [47, 0]
topLeft = [47 - imageStep, 0] 
topExtraLeft = [47 - imageStep * 2, 0] 
topRight = [47 + imageStep, 0]
topExtraRight = [47 + imageStep * 2, 0]
bottomLeft = [47 - imageStep, 13]
bottomExtraLeft = [47 - imageStep * 2, 13]
bottomRight = [47 +  imageStep, 13]
bottomExtraRight = [47 +  imageStep * 2, 13]


def getComments(imgPath):
    comments = {}
    with open(imgPath, 'rb') as imgFile:
        try:
            for line in imgFile:
                line = line.decode('ascii')
                if (line == "P6\n"):
                    continue                
                if (not line.startswith("#")):
                    break
                
                comment = line.split(':', 1)
                key = comment[0][1:].strip()
                value = comment[1].strip()
                
                # Do a little type fixing
                if key in ['Score', 'ScoreConf']:
                    value = float(value)
                elif key == 'ScoreCount':
                    value = int(value)
                            
                comments[key] = value
        except:
            print(imgPath)
            print(sys.exc_info())
    
    if "Class" in comments:
        if "Score" not in comments:
            comments['Score'] = classMap[comments["Class"]]
            comments['ScoreCount'] = 1
            comments['ScoreConf'] = 1.0
    
    return comments
    
def setComments(imagePath, comments):
    img = open(imagePath, "rb")
    lines = img.readlines()
    img.close();
    
    start = None
    stop = None
    for index, line in enumerate(lines):
        line = line.decode('ascii')
        if line.strip() == "P6":
            start = index + 1
            continue
        
        if start != None and stop == None:
            if line.startswith("#"):
                continue
            
            stop = index
            break
    
    header = []
    for comment in comments:
        header.append(bytearray("#"+comment+": "+str(comments[comment])+"\n", "ascii"))
    
    lines = lines[0:start]+header+lines[stop:]
    
    # WRite the new file
    img = open(imagePath, "wb")
    for line in lines:
        img.write(line)
    img.close()

def getCompletedImages(rootPath):
    ppmImages = []
    for root, dirs, files, in os.walk(rootPath):
        for f in files:
            if f.endswith(".ppm"):
                ppmImages.append(os.path.join(root, f)) 
    return ppmImages

def getClassCounts(dirPath):
    from collections import defaultdict
    classCounts = defaultdict(int)
    for imagePath in getCompletedImages(dirPath):
        comments = getComments(imagePath)
        if not "Class" in comments:
            continue
        
        classCounts[comments["Class"]] += 1
    return classCounts.items()

def getClassFromScore(score):
    if score > 0.8:
        return "B"
    elif score > 0.35:
        return "R"
    elif score > 0.15:
        return "SR"
    elif score > -0.15:
        return "F"
    elif score > -0.35:
        return "SL"
    elif score > -0.8:
        return "L"
    elif score > -1.1:
        return "B"
    else:
        return "Bad"

def displayImage(imagePath):
    print(imagePath)

    img = Image.open(imagePath)
    display(img)
    print(img.width, img.height)

    print(getComments(imagePath))
        
def copyImage(sourceFile, destFile):
    copyfile(sourceFile, destFile)
    
    comments = getComments(destFile)    
    setComments(destFile, comments)    
        
def mergeImage(sourceFile, destFile):
    sourceComments = getComments(sourceFile)    
    destComments = getComments(destFile)
    
    # Update comments
    destScore = destComments["Score"] * 180
    sourceScore = sourceComments["Score"] * 180
    distance = 1 - abs(getDistance(destScore, sourceScore) / 180)
    score = meanAngle([destScore, sourceScore]) / 180    
    
    destComments["ScoreConf"] = (destComments["ScoreConf"] + distance) / 2    
    destComments["Score"] = score
    destComments["ScoreCount"] += 1
    destComments["Class"] = getClassFromScore(destComments["Score"])
    
    setComments(destFile, destComments)

# https://rosettacode.org/wiki/Averages/Mean_angle#Python
def meanAngle(deg):
    return degrees(phase(sum(rect(1, radians(d)) for d in deg)/len(deg)))

#https://stackoverflow.com/questions/36000400/calculate-difference-between-two-angles
def getDistance(unit1, unit2):
    phi = abs(unit2-unit1) % 360
    sign = 1
    # used to calculate sign
    if not ((unit1-unit2 >= 0 and unit1-unit2 <= 180) or (
            unit1-unit2 <= -180 and unit1-unit2 >= -360)):
        sign = -1
    if phi > 180:
        result = 360-phi
    else:
        result = phi

    return result*sign

def flipImage(imagePath, destDir):
    comments = getComments(imagePath)
    filename, fileext = os.path.splitext(imagePath)
    filename = os.path.basename(filename)
    
    if comments["Class"] == "SL":
        comments["Class"] = "SR"
    elif comments["Class"] == "SR":
        comments["Class"] = "SL"
    elif comments["Class"] == "L":
        comments["Class"] = "R"
    elif comments["Class"] == "R":
        comments["Class"] = "L"
    
    img = Image.open(imagePath)
    
    flipped = img.transpose(Image.FLIP_LEFT_RIGHT)
    
    flippedFile = os.path.join(destDir, filename + "_flipped" + fileext)    
    flipped.save(flippedFile)
    setComments(flippedFile, comments)  
    
    return flippedFile

def augmentImage(imagePath, destDir):
    comments = getComments(imagePath)
    filename, fileext = os.path.splitext(imagePath)
    filename = os.path.basename(filename)
    
    img = Image.open(imagePath)
    
    # Center
    centerFile = os.path.join(destDir, filename + "_c" + fileext)    
    centerCrop = img.crop((center[0], center[1], 227 + center[0], 227 + center[1]))
    centerCrop.save(centerFile)    
    setComments(centerFile, comments)
    
    # Top left
    #topLeftFile = os.path.join(destDir, filename + "_tl" + fileext)    
    #topLeftCrop = img.crop((topLeft[0], topLeft[1], 227 + topLeft[0], 227 + topLeft[1]))
    #topLeftCrop.save(topLeftFile)    
    #setComments(topLeftFile, comments)
    
    # Top Extra left
    topExtraLeftFile = os.path.join(destDir, filename + "_tel" + fileext)    
    topExtraLeftCrop = img.crop((topExtraLeft[0], topExtraLeft[1], 227 + topExtraLeft[0], 227 + topExtraLeft[1]))
    topExtraLeftCrop.save(topExtraLeftFile)    
    setComments(topExtraLeftFile, comments)
    
    # Top Right
    #topRightFile = os.path.join(destDir, filename + "_tr" + fileext)    
    #topRightCrop = img.crop((topRight[0], topRight[1], 227 + topRight[0], 227 + topRight[1]))
    #topRightCrop.save(topRightFile)    
    #setComments(topRightFile, comments)
    
    # Top Extra left
    topExtraRightFile = os.path.join(destDir, filename + "_ter" + fileext)    
    topExtraRightCrop = img.crop((topExtraRight[0], topExtraRight[1], 227 + topExtraRight[0], 227 + topExtraRight[1]))
    topExtraRightCrop.save(topExtraRightFile)    
    setComments(topExtraRightFile, comments)
    
    # Bottom Left
    #bottomLeftFile = os.path.join(destDir, filename + "_bl" + fileext)    
    #bottomLeftCrop = img.crop((bottomLeft[0], bottomLeft[1], 227 + bottomLeft[0], 227 + bottomLeft[1]))
    #bottomLeftCrop.save(bottomLeftFile)    
    #setComments(bottomLeftFile, comments)
    
    # Botto Extra Left
    bottomExtraLeftFile = os.path.join(destDir, filename + "_bel" + fileext)    
    bottomExtraLeftCrop = img.crop((bottomExtraLeft[0], bottomExtraLeft[1], 227 + bottomExtraLeft[0], 227 + bottomExtraLeft[1]))
    bottomExtraLeftCrop.save(bottomExtraLeftFile)    
    setComments(bottomExtraLeftFile, comments)
    
    # Bottom Right
    #bottomRightFile = os.path.join(destDir, filename + "_br" + fileext)    
    #bottomRightCrop = img.crop((bottomRight[0], bottomRight[1], 227 + bottomRight[0], 227 + bottomRight[1]))
    #bottomRightCrop.save(bottomRightFile)    
    #setComments(bottomRightFile, comments)
    
    # Bottom Extra Right
    bottomExtraRightFile = os.path.join(destDir, filename + "_ber" + fileext)    
    bottomExtraRightCrop = img.crop((bottomExtraRight[0], bottomExtraRight[1], 227 + bottomExtraRight[0], 227 + bottomExtraRight[1]))
    bottomExtraRightCrop.save(bottomExtraRightFile)    
    setComments(bottomExtraRightFile, comments)
    

def writeClassFile(classFilePath, dirPath):
    classFile = open(classFilePath, 'w+')

    images = getCompletedImages(dirPath)
    random.shuffle(images)

    for imagePath in images:
        comments = getComments(imagePath)
        classFile.write(imagePath + " " + str(classNumber[comments["Class"]]) + "\n")

    classFile.close()

def augmentRedux(sourceDir, destDir):
    NUM_AUGS = 5
    seq = iaa.Sequential([
        iaa.SomeOf((1, 3), [
            iaa.OneOf([
                iaa.GaussianBlur(sigma=(0, 2)),
                iaa.AverageBlur(k=(2, 7)),
                iaa.MedianBlur(k=(3, 11)),
            ]),
            iaa.ContrastNormalization((0.75, 1.5)),
            iaa.Multiply((0.8, 1.2), per_channel=0.2),
            iaa.Superpixels(
                p_replace=(0, 1.0),
                n_segments=(20, 200)
            ),
            iaa.EdgeDetect(alpha=(0, 0.7)),
            iaa.AdditiveGaussianNoise(
                loc=0, scale=(0.1, 0.05*255), per_channel=0.5
            ),
            iaa.ContrastNormalization((0.5, 2.0), per_channel=0.5),
            iaa.Grayscale(alpha=(0.0, 1.0)),
            iaa.Multiply((0.5, 1.5), per_channel=0.5),
            iaa.Add((-20, 20), per_channel=0.5),
            iaa.Invert(0.05, per_channel=True),
            iaa.OneOf([
              iaa.Dropout((0.01, 0.1), per_channel=0.5),
              iaa.CoarseDropout(
                  (0.03, 0.15), size_percent=(0.02, 0.05), per_channel=0.2),
            ]),
            iaa.Sharpen(alpha=(0, 1.0), lightness=(0.75, 1.5)),
        ], random_order=True),
    ], random_order=True)
    
    imagePaths = getCompletedImages(sourceDir)
    for imagePath in imagePaths:
        comments = getComments(imagePath)
        filename, fileext = os.path.splitext(imagePath)
        filename = os.path.basename(filename)
        
        img = Image.open(imagePath)
        imgData = np.array(img)
        
        auged = seq.augment_images(np.array(NUM_AUGS * [imgData]))
        for idx, aug in enumerate(auged):
            augFile = os.path.join(destDir, filename + "_redux" + str(idx) + fileext)
            
            if os.path.exists(augFile):
                continue
          
            augImage = Image.fromarray(aug)
            augImage.save(augFile)
            setComments(augFile, comments)
            #display(augImage)

def writeClassFile(classFile, incomingDir):    
    destFile = open(classFile, "w+", encoding="ascii")
    
    imageFiles = getCompletedImages(incomingDir)
    for incomingFile in imageFiles:
        filename, ext = os.path.splitext(incomingFile)
        filename = os.path.basename(filename)
        outgoingFile = os.path.join(incomingDir, filename + ".png")
        
        comments = getComments(incomingFile)
        
        if "Class" not in comments:
            continue
        
        print(comments)
        
        fileClass = comments["Class"]
        if  fileClass in ["Bad", "BR"]:
            continue
        
        if not os.path.exists(outgoingFile):
            img = Image.open(incomingFile)
            imgResized = img.resize((227, 227))
            imgResized.save(outgoingFile)
        
        destFile.write(outgoingFile + " " + str(classNumber[fileClass]) + "\n")
        
    destFile.close()

def sampleData(sourceDir, destDir, sampleSize):
    classes = {"B": [], "R": [], "SR": [], "F": [], "SL": [], "L": []}
    
    images = getCompletedImages(sourceDir)
    for imagePath in images:
        comments = getComments(imagePath)
        classes[comments["Class"]].append(imagePath)
        
    for cls in classes:
        for choice in classes[cls][0:sampleSize]:
            filename = os.path.basename(choice)
            copyfile(choice, os.path.join(destDir, filename))
            
    print(getClassCounts(devPath))
    
def getRandomImageFromText(txtFile):
    txt = open(txtFile, 'r')
    lines = txt.readlines()
    random.shuffle(lines)
    
    return lines[0].strip().split(' ')
