#include <pxHttpCache.h>
#include <string.h>
#include <curl/curl.h>
#include <sstream>
#include "rtLog.h"

rtHttpCacheData::rtHttpCacheData():mExpirationDate(0),mUpdated(false),mDownloadRequest(NULL),mDownloadFailed(false)
{
#ifndef USE_STD_THREADS
  pthread_mutex_init(&mMutex, NULL);
  pthread_cond_init(&mCond, NULL);
#endif
  fp = NULL;
}

rtHttpCacheData::rtHttpCacheData(const char* url):mUrl(url),mExpirationDate(0),mUpdated(false),mDownloadRequest(NULL),mDownloadFailed(false)
{
#ifndef USE_STD_THREADS
  pthread_mutex_init(&mMutex, NULL);
  pthread_cond_init(&mCond, NULL);
#endif
  fp = NULL;
}

rtHttpCacheData::rtHttpCacheData(const char* url, const char* headerMetadata, const char* data, int size):mUrl(url),mExpirationDate(0),mUpdated(false),mDownloadRequest(NULL),mDownloadFailed(false)
{
#ifndef USE_STD_THREADS
  pthread_mutex_init(&mMutex, NULL);
  pthread_cond_init(&mCond, NULL);
#endif
  if ((NULL != headerMetadata) && (NULL != data))
  {
    mHeaderMetaData.init(headerMetadata,strlen(headerMetadata));
    populateHeaderMap();
    setExpirationDate();
    mData.init(data,size);
  }
  fp = NULL;
}

rtHttpCacheData::~rtHttpCacheData()
{
#ifndef USE_STD_THREADS
    pthread_mutex_destroy(&mMutex);
    pthread_cond_destroy(&mCond);
#endif
    mDownloadRequest = NULL;
  fp = NULL;
  mDownloadFailed = false;
}

void rtHttpCacheData::populateHeaderMap()
{
  int pos=0,prevpos = 0;
  string headerString = mHeaderMetaData.data();
  pos = headerString.find_first_of("\n",0);
  string attribute = headerString.substr(prevpos,(pos = headerString.find_first_of("\n",prevpos))-prevpos);
  do
  {
    if (attribute.size() >  0)
    {
      prevpos = pos+1;

      //parsing the header attribute and value pair
      string key(""),value("");
      int name_end_pos = attribute.find_first_of(":");
      if (name_end_pos == string::npos)
      {
        key = attribute; 
      }
      else
      {
        key = attribute.substr(0,name_end_pos);
      }
      int cReturn_nwLnPos  = key.find_first_of("\r");
      if (string::npos != cReturn_nwLnPos)
        key.erase(cReturn_nwLnPos,1);
      cReturn_nwLnPos  = key.find_first_of("\n");
      if (string::npos != cReturn_nwLnPos)
        key.erase(cReturn_nwLnPos,1);
      if (name_end_pos == string::npos)
      {
        mHeaderMap.insert(std::pair<rtString, rtString>(key.c_str(),rtString("")));
      }
      else
      {
       value = attribute.substr(name_end_pos+1,attribute.length());
       cReturn_nwLnPos  = value.find_first_of("\r");
       if (string::npos != cReturn_nwLnPos)
         value.erase(cReturn_nwLnPos,1);
       cReturn_nwLnPos  = value.find_first_of("\n");
       if (string::npos != cReturn_nwLnPos)
         value.erase(cReturn_nwLnPos,1);
       mHeaderMap.insert(std::pair<rtString, rtString>(key.c_str(),value.c_str()));
      }
    }
    attribute = headerString.substr(prevpos,(pos = headerString.find_first_of("\n",prevpos))-prevpos);
  } while(pos != string:: npos);
}

rtString rtHttpCacheData::expirationDate()
{
  char buffer[100];
  memset(buffer,0,100);
  strftime(buffer, 100, "%Y-%m-%d %H:%M:%S", localtime(&mExpirationDate));
  return rtString(buffer);
}

time_t rtHttpCacheData::expirationDateUnix()
{
  return mExpirationDate;
}

bool rtHttpCacheData::isExpired()
{
  if (0 == mExpirationDate)
    return true;

  time_t now = time(NULL);
  if (now >= mExpirationDate)
    return true;
  return false;
}

bool rtHttpCacheData::isValid()
{
  if ((mData.length() != 0)  && !isExpired())
  {
    return true;
  }
  return false;
}

bool rtHttpCacheData::isWritableToCache()
{
  // need to add more  conditions ???
  if (isValid())
  {
    string cacheControl = mHeaderMap["Cache-Control"].cString();
    if (string::npos != cacheControl.find("no-store"))
    {
      return false;
    }
    return true;
  }
  return false;
}

void rtHttpCacheData::setAttributes(const char* rawAttributes)
{
  mHeaderMetaData.init(rawAttributes,strlen(rawAttributes));
  populateHeaderMap();
  setExpirationDate();
}

rtError rtHttpCacheData::attributes(map<rtString, rtString>& cacheAttributes)
{
  cacheAttributes = mHeaderMap;
  return RT_OK;
}

rtData& rtHttpCacheData::getHeaderData()
{
  return mHeaderMetaData;
}

rtData& rtHttpCacheData::getContentsData()
{
  return mData;
}

rtError rtHttpCacheData::data(rtData& data)
{
  if (NULL == fp)
    return RT_ERROR;

  populateExpirationDateFromCache();

  bool revalidate =  false;
  bool revalidateOnlyHeaders = false;

  rtError res;
  res = calculateRevalidationNeed(revalidate,revalidateOnlyHeaders);

  if (RT_OK != res)
    return res;

  if (true == revalidate)
    return performRevalidation(data);

  if (true == revalidateOnlyHeaders)
  {
    if (RT_OK != performHeaderRevalidation())
      return RT_ERROR;
  }

  if (mHeaderMap.end() != mHeaderMap.find("ETag"))
  {
    rtError res =  handleEtag(data);
    if (RT_OK != res)
      return RT_ERROR;
    if (mUpdated)
    {
      return RT_OK;
    }
  }

  if (false == readFileData())
    return RT_ERROR;

  data.init(mData.data(),mData.length());
  if (true == revalidateOnlyHeaders)
  {
    mUpdated = true; //headers  modified , so rewriting the cache with new header data
  }
  return RT_OK;
}

void rtHttpCacheData::setData(const rtData& cacheData)
{
  mData.init(cacheData.data(),cacheData.length());
}

rtError rtHttpCacheData::url(rtString& url)
{
  url = mUrl;
  return RT_OK;
}

rtError rtHttpCacheData::etag(rtString& tag) //returns the etag (if available)
{
  if (mHeaderMap.end() != mHeaderMap.find("ETag"))
  {
    tag = mHeaderMap["ETag"];
    return RT_OK;
  }
  return RT_ERROR;
}

bool rtHttpCacheData::isUpdated()
{
  return mUpdated;
}

void rtHttpCacheData::setFilePointer(FILE* openedDescriptor)
{
  fp = openedDescriptor;
}

void rtHttpCacheData::setExpirationDate()
{
  string expirationDate = "";
  bool foundMaxAge = false;
  if (mHeaderMap.end() != mHeaderMap.find("Cache-Control"))
  {
    string cacheControl = mHeaderMap["Cache-Control"].cString();
    int pos = cacheControl.find("max-age");
    if (string::npos != pos)
    {
      foundMaxAge = true;
      string maxAge = cacheControl.substr(pos+8);
      long int maxAgeInt = 0;
      stringstream stream(maxAge);
      stream >> maxAgeInt;
      mExpirationDate = time(NULL) + maxAgeInt;
    }
  }
  if (false == foundMaxAge) 
  {
    if (mHeaderMap.end() != mHeaderMap.find("Expires"))
    {
      struct tm timeinfo;
      memset(&timeinfo,0,sizeof(struct tm));
      strptime(mHeaderMap["Expires"].cString(), " %a, %d %b %Y %H:%M:%S %Z", &timeinfo);
      mExpirationDate = timegm(&timeinfo);
    }
  }
}

// Static callback that gets called when fileDownloadRequest completes
void rtHttpCacheData::onDownloadComplete(pxFileDownloadRequest* fileDownloadRequest)
{
  if (fileDownloadRequest != NULL && fileDownloadRequest->getCallbackData() != NULL)
  {
    rtHttpCacheData* callbackData = (rtHttpCacheData*) fileDownloadRequest->getCallbackData();
    if (NULL != callbackData)
    {
      if (fileDownloadRequest->getDownloadStatusCode() == 0 &&
          fileDownloadRequest->getHttpStatusCode() == 200)
      {
        if (fileDownloadRequest->getHeaderData() != NULL)
          callbackData->mHeaderMetaData.init(fileDownloadRequest->getHeaderData(), fileDownloadRequest->getHeaderDataSize());
        if (fileDownloadRequest->getDownloadedData() != NULL)
        {
          callbackData->mData.init(fileDownloadRequest->getDownloadedData(), fileDownloadRequest->getDownloadedDataSize());
          callbackData->mUpdated = true;
        }
      }

      if (fileDownloadRequest->getDownloadStatusCode() != 0)
      {
        callbackData->mDownloadFailed = true;
      }
      #ifdef USE_STD_THREADS
      callbackData->mCond.notify_one();
      #else
      pthread_mutex_unlock(&callbackData->mMutex);
      pthread_cond_signal(&callbackData->mCond);
      #endif
    }
  }
}

rtError rtHttpCacheData::calculateRevalidationNeed(bool& revalidate, bool& revalidateOnlyHeaders)
{
  if (isExpired())
  {
    if (mHeaderMap.end() != mHeaderMap.find("Cache-Control"))
    {
      string cacheControl = mHeaderMap["Cache-Control"].cString();
      int pos = cacheControl.find("must-revalidate");
      if (string::npos != pos)
      {
        revalidate = true;
        return RT_OK;
      }
      else
        return RT_ERROR; //expired cache data and need to be reloaded again
    }
  }

  if (mHeaderMap.end() != mHeaderMap.find("Cache-Control"))
  {
    string cacheControl = mHeaderMap["Cache-Control"].cString();
    int pos = 0,prevpos = 0;
    while ((pos = cacheControl.find("no-cache",prevpos)) != string::npos)
    {
       //no-cache=<parameter>
       printf("[%d]  [%s] [%c] \n",pos,  cacheControl.c_str(),cacheControl.at(pos+8));
       fflush(stdout);
       if (cacheControl.at(pos+8) == '=')
       {
         int noCacheEnd =  cacheControl.find_first_of(",",pos+9);
         string parameter;
         // no-cache can be last parameter
         if (string::npos == noCacheEnd)
         {
           parameter = cacheControl.substr(pos+9);
         }
         else
         {
           parameter = cacheControl.substr(pos+9,noCacheEnd - (pos + 9));
         }
         printf("Erasing header [%s] \n",parameter.c_str());
         fflush(stdout);
         mHeaderMap.erase(parameter.c_str());
         revalidateOnlyHeaders = true;
         if (string::npos == noCacheEnd)
           break;
         prevpos = noCacheEnd;
       }
       else
       {
         //Revalidate the full contents, so download it completely newer
         revalidate = true;
         break;
       }
    }
  }
  return RT_OK;
}

bool rtHttpCacheData::handleDownloadRequest(vector<rtString>& headers,bool downloadBody)
{
  pxFileDownloadRequest* mDownloadRequest = NULL;
  mDownloadRequest = new pxFileDownloadRequest(mUrl, this);
  // setup for asynchronous load and callback
  mDownloadRequest->setCallbackFunction(rtHttpCacheData::onDownloadComplete);
  mDownloadRequest->setAdditionalHttpHeaders(headers);
  if (!downloadBody)
    mDownloadRequest->setHeaderOnly(true);
  pxFileDownloader::getInstance()->addToDownloadQueue(mDownloadRequest);
  #ifdef USE_STD_THREADS
  std::unique_lock<std::mutex> lock(mMutex);
  mCond.wait(lock);
  #else
  pthread_mutex_lock(&mMutex);
  pthread_cond_wait(&mCond, &mMutex);
  pthread_mutex_unlock(&mMutex);
  #endif
  if (mDownloadFailed)
     return false;
  return true;
}

bool rtHttpCacheData::readFileData()
{
  char *contentsData = NULL;
  char buffer[100];
  int bytesCount = 0;
  int totalBytes = 0;
  while (!feof(fp))
  {
    bytesCount = fread(buffer,1,100,fp);
    if (NULL == contentsData)
      contentsData = (char *)malloc(bytesCount);
    else
      contentsData = (char *)realloc(contentsData,totalBytes+bytesCount);
    if (NULL == contentsData)
    {
      fclose(fp);
      return false;
    }
    memcpy(contentsData+totalBytes,buffer,bytesCount);
    totalBytes += bytesCount;
    memset(buffer,0,100);
  }
  fclose(fp);
  if (NULL != contentsData)
  {
    mData.init(contentsData,totalBytes);
    free(contentsData);
    contentsData = NULL;
  }
  return true;
}

void rtHttpCacheData::populateExpirationDateFromCache()
{
  char buf;
  string date;
  while ( !feof(fp) )
  {
    buf = fgetc(fp);
    if (buf == '|')
    {
      break;
    }
    date.append(1,buf);
  }

  stringstream stream(date);
  stream >> mExpirationDate;
}

rtError rtHttpCacheData::performRevalidation(rtData& data)
{
  rtString headerOption = "Cache-Control: max-age=0";
  vector<rtString> headers;
  headers.push_back(headerOption);

  if (!handleDownloadRequest(headers))
  {
    return RT_ERROR;
  }

  if (mUpdated)
  {
    populateHeaderMap();
    setExpirationDate();
    data.init(mData.data(),mData.length());
    fclose(fp);
    return RT_OK;
  }
  else
  {
    return RT_ERROR;
  }
}

rtError rtHttpCacheData::performHeaderRevalidation()
{
  rtString headerOption = "Cache-Control: max-age=0";
  vector<rtString> headers;
  headers.push_back(headerOption);

  if (!handleDownloadRequest(headers,false))
  {
    return RT_ERROR;
  }

  populateHeaderMap();
  setExpirationDate();
  return RT_OK;
}

rtError rtHttpCacheData::handleEtag(rtData& data)
{
  rtString headerOption = "If-None-Match:";
  headerOption.append(mHeaderMap["ETag"].cString());
  vector<rtString> headers;
  headers.push_back(headerOption);

  if (!handleDownloadRequest(headers))
  {
    return RT_ERROR;
  }

  if (mUpdated)
  {
    populateHeaderMap();
    setExpirationDate();
    data.init(mData.data(),mData.length());
    fclose(fp);
  }
  return RT_OK;
}