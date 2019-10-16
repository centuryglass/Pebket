var debugPageList = false;
var debugPageText = false;
var debugConnection = false;
var debug = false;//Enables debug text

//----------LOCAL VALUE DEFINITIONS----------


var JS_MESSAGE_CODES = {sendingPageTitles:0,
                        sendingPageText:1,
                        JSInitialized:2,
                        resetPageData:3,
                        opSucceeded:4,
                        opFailed:5};
var PEBBLE_MESSAGE_CODES = {requestingPageTitles:0,
                            loadPage:1,
                            getPageText:2,
                            archivePage:3,
                            deletePage:4,
                            favoritePage:5,
                            clearLocalStorage:6,
                            updateTitles:7,
                            bookmarkPage:8,
                            removeBookmark:9};

var OPCODES = {login:0,
               loadPages:1,
               loadText:2,
               toggleFave:3,
               toggleArchive:4,
               toggleDelete:5};

var PAGE_STATE_VALUES = ["unread", "archive", "all"];

var FAVE_STATUS_VALUES = [0,1,null];

var SORT_ORDER_VALUES = ["newest","oldest","title","site"];


//----------PAGE LIST DATA----------
//PocketConnection connection: handles connecting to pocket API
var connection;

//PageLists savedPageLists:list of saved pages
var savedPageLists;

//SavedPage savedPage: handles stored page text
var savedPage;


/**
*sends a message to pebble indicating that 
*an operation finished
*opSucceeded: true if the operation was a success, false
*if it failed
*message: information about the completed message
*opcode: code indicating which operation finished
*/
function sendResultMessage(opSucceeded,message,opcode){
  var code;
  if(opSucceeded) code = JS_MESSAGE_CODES.opSucceeded;
  else code = JS_MESSAGE_CODES.opFailed;
  var msg = {'message_code':code,
             'message_text':message,
             'opcode':opcode};
  Pebble.sendAppMessage(msg);
}

//Gets the current time in a format compatible
//with pocket requests
//(10 char unix time string)
function pocketTime(){
  return Date.now().toString().substr(0,10);
}

//Gets a formatted time string from a duration in ms
function getTimeString(ms){
  ms -= ms % 1000;
  var sec = ms/1000;
  var min = 0;
  var hour = 0;
  if(sec > 60){
    min = ((sec - (sec % 60))/60);
    sec = sec % 60;
  }
  if(min > 60){
    hour = ((min - (min % 60))/60);
    min = min % 60;
  }
  var result = "";
  if(hour > 0){
    result += hour + " hour";
    if(hour > 1) result += "s";
    result +=", ";
  }
  result += min + " minute";
  if(min != 1) result += "s";
  result += ", "+sec+" second";
  if(sec != 1) result += "s";
  return result;
}

//----------CONNECTION----------
//Handles connecting to pocket
function PocketConnection(pocketKey){
  this.CONSUMER_KEY = "INSERT_KEY_HERE"; //Pocket API key

  this.AUTH_PAGE = "https://getpocket.com/auth/authorize"; //Pocket authorization page
  this.REDIRECT_URI = 'pebblejs://close#success';
  //redirect URI to pass to Pocket for authentication
  this.REQUEST_TOKEN_URI = "https://getpocket.com/v3/oauth/request";
  //Address for obtaining an authorization request token
  this.AUTH_TOKEN_URI = "https://getpocket.com/v3/oauth/authorize";
  //Address for obtaining an authorization token
  this.GET_URI = "https://getpocket.com/v3/get";
  //Address for getting saved page data
  this.SEND_URI = "https://getpocket.com/v3/send";
  //Address for changing page data
  
  /**
  *saves connection data to local storage
  *return: true if data is saved, false otherwise
  */
  this.save = function(){ 
    try{
      localStorage.setItem(pocketKey,JSON.stringify(this));
      if(debugConnection)console.log("PocketConnection.save: saved connection data of size "+JSON.stringify(this).length);
    }catch(err){
      if(debugConnection)console.log("PocketConnection.save: error,"+err);
    }
  };
  
  /**
  *Loads connection data from local storage
  *returns true if data was found, false if not
  */
  this.load = function(){
  var connectionData = localStorage.getItem(this.pocketKey);
    if(connectionData){ 
      connectionData = JSON.parse(connectionData);
      if(debugConnection)console.log("PocketConnection.load:loaded "+pocketKey+" from storage");
      for(var key in connectionData){
        if(key != "request_token")
          this[key] = connectionData[key];
      }
      if(debugConnection)console.log("PocketConnection.load: loaded pocket connection of size "+JSON.stringify(this).length);
      return true;
    }
    if(debugConnection)console.log("PocketConnection.load:didn't find "+pocketKey+" in storage");
    return false;
  };
  
  /**
  *Opens a connection to Pocket
  *uri: the address of the new connection
  *return: the connection, an XMLHttpRequest
  */
  this.openPocketConnection = function(uri){ 
    if(debugConnection)console.log("openPocketConnection: Connecting to "+uri);
    var connection = new XMLHttpRequest();
    connection.open("POST", uri, true);
    connection.setRequestHeader("Content-Type", "application/json; charset=UTF8");
    connection.setRequestHeader("X-Accept", "application/json");
    return connection;
  };
  
  
  /**
  *Checks pocket connection limits,
  *disabling connections if necessary
  */
  this.updateConnectionLimits = function(connection){
    var connectionTime = Date.now();
    var userLimitRemaining = connection.getResponseHeader("X-Limit-User-Remaining");
    var keyLimitRemaining = connection.getResponseHeader("X-Limit-Key-Remaining");
    if(debugConnection)console.log("updateConnectionLimits: user="+userLimitRemaining+" key="+keyLimitRemaining);
    var secUntilReset = 0;
    if(userLimitRemaining !== null && userLimitRemaining < 5){
      secUntilReset = connection.getResponseHeader("X-Limit-User-Reset");
      if(debugConnection)console.log("reaching user limit, disabling connections for "+ getTimeString(secUntilReset * 1000));
    }
    if(keyLimitRemaining !== null &&  keyLimitRemaining < 20){
      var keyReset = connection.getResponseHeader("X-Limit-Key-Reset");
      if(keyReset > secUntilReset)secUntilReset = keyReset;
      if(debugConnection)console.log("reaching key limit, disabling connections for " + getTimeString(secUntilReset * 1000));
    }
    if(secUntilReset > 0) this.disableUntil = connectionTime + (secUntilReset * 1000);
  };
  
    
  /**
  *sends a request to pocket
  *request: request parameters
  *uri: request uri
  *callback: callback(XMLHttpRequest) runs when connection loads
  */
  this.sendRequest = function(request,uri,callback){
    if(this.disableUntil){
      var timeRemaining = this.disableUntil - Date.now();
      if(timeRemaining > 0){
        if(debugConnection)console.log("sendRequest: failed, connections will be allowed in "+getTimeString(timeRemaining));
        sendResultMessage(false,"Connection limit reached: Pocket connections blocked for "+getTimeString(timeRemaining),null);
        return null;
      }
      else{
        this.disableUntil = null;
        if(debugConnection)console.log("sendRequest: connection limit has reset, allowing connections again");
      }
    }
    var pocketManager = this;
    var connection = this.openPocketConnection(uri);
    connection.onload = function(){
      pocketManager.updateConnectionLimits(this);
      callback(this);
    };
    request.consumer_key = this.CONSUMER_KEY;
    if(debugConnection)console.log("sendRequest: Sending pocket request to "+uri);
    if(debugConnection)console.log("sendRequest: request content: "+JSON.stringify(request));
    connection.send(JSON.stringify(request));
  };
  
  /**
  *Gets and saves a request token for authentication
  *callback: optional callback function, passes the pocket connection as an argument
  */
  this.getRequestToken = function(callback){
    if(debugConnection)console.log("getRequestToken: getting request token");
    var request = {"redirect_uri":this.REDIRECT_URI };
    var uri = this.REQUEST_TOKEN_URI;
    var pocketCon = this;
    var requestCall = function(xmlhttp){
      try{
        var response = JSON.parse(xmlhttp.response);
        if(response.code){
          pocketCon.request_token = response.code;
          if(debugConnection)console.log("getRequestToken: request token is " + pocketCon.request_token);
        }
        else if(debugConnection)console.log("getRequestToken: error: " + xmlhttp.response);
        if(callback) callback(pocketCon);
      }
      catch(err){
        if(debugConnection)console.log("getRequestToken: x-error: " + xmlhttp.getResponseHeader("x-error"));
      }
    };
    this.sendRequest(request,uri,requestCall);
  };

  /**
  *Gets and saves an access token for authentication
  *callback: optional callback(pocketConnection) runs when token is found
  */
  this.getAccessToken = function(callback){
    if(debugConnection)console.log("getAccessToken: getting access token");
    if(!this.request_token){
      this.getRequestToken();
      if(debugConnection)console.log("getAccessToken: request token not found, getting that instead");
      sendResultMessage(false,"Connect your Pocket account in the app settings page on your phone.",OPCODES.login);
    }
    else{
      var request = {"consumer_key":this.CONSUMER_KEY, "code":this.request_token};
      var uri = this.AUTH_TOKEN_URI;
      var pocket = this;
      var call = function(xmlhttp,args){
        try{
          var response = JSON.parse(xmlhttp.response);
          if(response.username){
            pocket.username = response.username;
            if(debugConnection)console.log("getAccessToken: username is " + pocket.username);
          }
          if(response.access_token){
            pocket.access_token = response.access_token;
            if(debugConnection)console.log("getAccessToken: access token is " + pocket.access_token);
            pocket.save();
            if(callback)callback(pocket);
            sendResultMessage(true,"Logged in as "+response.username,OPCODES.login);
          }
          else{
            if(debugConnection)console.log("error: " + xmlhttp.response);
            if(!pocket.access_token)
              sendResultMessage(false,"Login failed!",OPCODES.login);
          }
        }
        catch(err){
          if(debugConnection)console.log("getAccessToken: x-error: " + xmlhttp.getResponseHeader("x-error"));
          if(!pocket.access_token)
            sendResultMessage(false,"Login failed! Connect your Pocket account in the app settings page on your phone.",OPCODES.login);
        }
      };
      this.sendRequest(request,uri,call);
    }
  };
  
   /**
  *Access the Pocket retrieve API
  *request: the request to make to the pocket server
  *optional params:
  *callback: callback(XMLHttpRequest) runs when connection loads
  */
  this.pocketRetrieve = function(request,callback){
    if(!this.access_token){//get access token if needed
      var pocketConnection = this;
      var call = function(){
        pocketConnection.pocketRetrieve(request,callback);
      };
      this.getAccessToken(call);
    }else{
      var uri = this.GET_URI;
      request.access_token = this.access_token;
      this.sendRequest(request,uri,callback);
    }
  };
  
  /**
  *Access the Pocket modify API
  *request: the request to make to the pocket server
  *optional params:
  *callback: callback(XMLHttpRequest) runs when connection loads
  */
  this.pocketModify = function(request,callback){
    if(!this.access_token){//get access token if needed
      var pocketConnection = this;
      var call = function(){
        pocketConnection.request(request,callback);
      };
      this.getAccessToken(call);
    }else{
      var uri = this.SEND_URI;
      request.access_token = this.access_token;
      this.sendRequest(request,uri,callback);
    }
  };
  
  /**
  *Call this to properly open the config page 
  */
  this.openConfig = function(){
    if(!this.request_token){
      var callback = function(pocketCon){
        pocketCon.openConfig();
      };
      this.getRequestToken(callback);
    }
    else{
      var url = this.AUTH_PAGE+
        "?request_token=" + this.request_token +
        "&redirect_uri="+ this.REDIRECT_URI;
      if(debugConnection)console.log('openConfig: opening '+url);
      Pebble.openURL(url);
    }
  };
 
  //Initialize values
  this.pocketKey = pocketKey;
  if(!this.load() || !this.access_token){
    this.getRequestToken();
    this.save();
  }
}

//----------PAGE LIST----------
//Handles lists of pages
function PageLists(pageKey,pocketConnection){
  this.PAGE_LIST_SIZE_LIMIT = 4 * 1024 * 1024;//limit: 4MB
  this.waitingForData = false; //True iff waiting on pocket data
  this.pageKey = pageKey;//localStorageKey
  this.pocketConnection = pocketConnection;//connection to pocket servers
  this.pageState = "unread";//pageState to request when loading pages
  this.favoriteStatus = null;//favoriteStatus to request when loading pages
  this.sortOrder = "newest";//sortOrder to request when loading pages
  //page title lists:
  this.savedPageList = [];
  this.archivedPageList = [];
  this.favoritePageList = [];
  this.modifyTime = pocketTime(); //last update time
  
   /**
  *saves list data to local storage
  *return: true if data is saved, false otherwise
  */
  this.save = function(){ 
    var saveData = {};
    for(var key in this){
      if(key != "pocketConnection") saveData[key] = this[key];
    }
    try{
      var itemsRemoved = 0;
      while(JSON.stringify(saveData).length > this.PAGE_LIST_SIZE_LIMIT){
        saveData.savedPageList.pop();
        saveData.archivedPageList.pop();
        itemsRemoved++;
      }
      if(debugPageList && itemsRemoved > 0)
        console.log("PageList.save: needed to remove "+itemsRemoved*2+" items from lists");
      localStorage.setItem(pageKey,JSON.stringify(saveData));
      if(debugPageList)console.log("PageList.save: saved pageLists of size "+JSON.stringify(saveData).length);
    }catch(err){
      if(debugPageList)console.log("PageList.save: error,"+err);
    }
  };
  
  /**
  *Loads list data from local storage
  *returns true if data was found, false if not
  */
  this.load = function(){
  var storedList = localStorage.getItem(this.pageKey);
    if(storedList){ 
      storedList = JSON.parse(storedList);
      if(debugPageList)console.log("PageLists.load:loaded "+pageKey+" from storage");
      for(var key in storedList){
        if(key != "pocketConnection")
          this[key] = storedList[key];
        else if(debugPageList)console.log("PageLists.load: ignoring key "+key);
      }
      if(debugPageList)console.log("PageList.load: loaded pageLists of size "+JSON.stringify(this).length);
      if(debugPageList)console.log("PageList.load: saved page list: "+this.savedPageList.length+" items, size "+JSON.stringify(this.savedPageList).length);
      
      if(debugPageList)console.log("PageList.load: archived page list: "+this.archivedPageList.length+" items, size "+JSON.stringify(this.archivedPageList).length);
      if(debugPageList)console.log("PageList.load: favorite page list: "+this.favoritePageList.length+" items, size "+JSON.stringify(this.favoritePageList).length);
      return true;
    }
    if(debugPageList)console.log("PageLists.load:didn't find "+pageKey+" in storage");
    return false;
  };
  
  /**
  *Sorts all page lists using the current sort format
  */
  this.sortAllLists = function(){
    //"newest","oldest","title","site"
    var sortFunction;
    if(this.sortOrder == "newest"){
      sortFunction = function(a,b){
        return b.time_added - a.time_added;
      };
    }
    else if(this.sortOrder == "oldest"){
      sortFunction = function(a,b){
        return a.time_added - b.time_added;
      }; 
    }
    else if(this.sortOrder == "title"){
      sortFunction = function(a,b){
        if(a.resolved_title && b.resolved_title){
          if(a.resolved_title < b.resolved_title)
            return -1;
          if(a.resolved_title > b.resolved_title)
            return 1;
        }
        return 0;
      }; 
    }
    else if(this.sortOrder == "site"){
      sortFunction = function(a,b){
        if(a.resolved_url && b.resolved_url){
          if(a.resolved_url < b.resolved_url)
            return -1;
          if(a.resolved_url > b.resolved_url)
            return 1;
        }
        return 0;
      }; 
    }
    if(sortFunction){
        this.savedPageList.sort(sortFunction);
        this.archivedPageList.sort(sortFunction);
        this.favoritePageList.sort(sortFunction);
    }
  };
  
  
  /**
  *Removes a page from a list
  *oldPageID: the item_id of the page to remove
  *pageList: the list to remove it from
  */
  this.deletePage = function(oldPageID, pageList){
    var pageIndex = 0;
    for(var page in pageList){
      if(pageList[page].item_id == oldPageID){
        pageList.splice(pageIndex,1);
        break;
      }else pageIndex++;
    }
  }; 
  
  /**
  *Updates a page in the list
  */
  this.updatePage = function(newPage){
    if(newPage.favorite == "0"){//item is not favorited
      this.deletePage(newPage.item_id,this.favoritePageList);
    }
    else if(newPage.favorite == "1"){//item is favorited
      this.deletePage(newPage.item_id,this.favoritePageList);
      this.favoritePageList.push(newPage);
    }
    if(newPage.status == "0"){//update in main list  
      this.deletePage(newPage.item_id,this.savedPageList);
      this.deletePage(newPage.item_id,this.archivedPageList);
      this.savedPageList.push(newPage);
    }
    else if(newPage.status == "1"){//update in archive
      this.deletePage(newPage.item_id,this.savedPageList);
      this.deletePage(newPage.item_id,this.archivedPageList);
      this.archivedPageList.push(newPage);
    }
    else if(newPage.status == "2"){//delete the page
      this.deletePage(newPage.item_id,this.savedPageList);
      this.deletePage(newPage.item_id,this.archivedPageList);
      this.deletePage(newPage.item_id,this.favoritePageList);
    }
  };
  
  /**
  *Load pages from the server
  *request: the request to make to the pocket server
  *callback: callback(PageLists) runs when connection loads
  */
  this.loadPages = function(request,callback){
    if(debugPageList)console.log("loadPages: Sending pocket request "+JSON.stringify(request));
    var pageLists = this;
    var call = function(XMLHttp){
      try{
        if(debugPageList)console.log("response recieved:"+XMLHttp.response);
        var response = JSON.parse(XMLHttp.response);
        var count = 0;
        for(var key in response.list){
          count++;
          if(request.offset)response.list[key].sort_id += request.offset;
          pageLists.updatePage(response.list[key]);
        }
        if(debugPageList)console.log("list size:"+count);
        pageLists.sortAllLists();
      }
      catch(err){
        if(debugPageList)console.log("loadPages: error: " + err);
        if(debugPageList)console.log("loadPages: x-error: " + XMLHttp.getResponseHeader("x-error"));
        sendResultMessage(false,"Loading page titles failed, error:"+XMLHttp.getResponseHeader("x-error"),OPCODES.loadPages);
      }
      pageLists.modifyTime = pocketTime();
      if(callback)callback(pageLists);
    };
    this.pocketConnection.pocketRetrieve(request,call);
  };
  
  /**
  *Check for and apply updates to pages
  *optional params:
  *callback: callback(PageLists) runs when connection loads
  */
  this.loadUpdates = function(callback){
    if(debugPageList)console.log("loadUpdates: finding updates since "+this.modifyTime);
    var request = {};
    request.sort = this.sortOrder;
    request.since = this.modifyTime;
    this.loadPages(request,callback);
  };
  
  /**
  *Load specified pages
  *index: first page to load
  *count: number of pages to load
  *optional params:
  *callback: callback(PageLists) runs when connection loads
  */
  this.loadNewPages = function(index,count,callback){
    if(debugPageList)console.log("loadNewPages: finding "+count+" pages at "+index);
    var request = {};
    request.count = count;
    request.offset = index;
    request.sort = this.sortOrder;
    request.state = this.pageState;
    if(this.favoriteStatus)request.favorite = this.favoriteStatus;
    this.loadPages(request,callback);
  };
  
  /**
  *Gets the currently selected page list
  *return: saved pages, archived pages, or favorite pages
  */
  this.getCurrentPageList = function(){
    var pageList;
    if(this.pageState == "archive"){
      console.log("getCurrentPageList: current list is: archive");
      pageList = this.archivedPageList;
    }
    else if(this.favoriteStatus == 1){
      pageList = this.favoritePageList;
      console.log("getCurrentPageList: current list is: favorite");
    }
    else{
      pageList = this.savedPageList;
      console.log("getCurrentPageList: current list is: saved pages");
    }
    return pageList;
  };
  
  /**
  *Send pages to Pebble
  *index: first page to send
  *count: number of pages to send
  *skipLoading: optional boolean signalling to skip loading new pages
  */
  this.pagesToPebble = function(index,count,skipLoading){
    if(debugPageList)console.log("pagesToPebble: pebble requested pages "+index+"-"+(index+count));
    var pageList = this.getCurrentPageList();
    if(!this.pebbleRequest && pageList.length < index+count && !skipLoading){
      if(debugPageList)console.log("pagesToPebble: list only contains "+pageList.length+" items, loading more");
      this.loadNewPages(index,count,function(pageLists){
          console.log("pagesToPebble:callback sending back request for "+count +" pages at "+index);
          pageLists.pagesToPebble(index,count,true);
      },this); 
    }else{//pages found, bundle titles into message
      this.pebbleRequest = null;
      var titleList = "";
      var titleNum = 0;
      for(var titleIndex = index; titleIndex < index+count; titleIndex++){
        if(pageList[titleIndex]){
          var titleItem = pageList[titleIndex];
          var title;
          if(titleItem.resolved_title) title = titleItem.resolved_title;
          else if(titleItem.given_title) title = titleItem.given_title;
          else if(titleItem.given_url) title = titleItem.given_url;
          if(title){
            titleNum++;
            titleList += title + '\n';
            //if(debugPageList)console.log("pagesToPebble: title " + titleIndex+ " is " + titleItem.resolved_title);
          }
        //else if(debugPageList)console.log("pagesToPebble:index "+titleNum+"had no resolved title, value:"+JSON.stringify(titleItem));
        }
      }
      if(debugPageList)console.log("pagesToPebble: found " +titleNum +  " titles, total length:" + titleList.length);
      if(titleNum > 0){
        var titleMsg = {};
        titleMsg.message_code = JS_MESSAGE_CODES.sendingPageTitles;
        titleMsg.message_text = titleList;
        titleMsg.item_count = titleNum-index;
        titleMsg.index = index;
        Pebble.sendAppMessage(titleMsg);
        this.save();
      }
    }
  };
  
  /**
  *gets a single page from the selected list
  *index: page index in the list
  *return: the selected page, if found
  */
  this.getPage = function(index){
    if(debugPageList)console.log("getPage: getting page "+index);
    var pageList = this.getCurrentPageList();
    if(pageList[index])return pageList[index];
  };
  
  /**
  *clears all saved page data, used if 
  *reloading because of changed sort order or something
  */
  this.clearLists = function(){
    if(debugPageList)console.log("clearLists: clearing all page data");
    var clearList = function(list){
      list.splice(0,list.length);
    };
    clearList(this.savedPageList);
    clearList(this.archivedPageList);
    clearList(this.favoritePageList);
    this.modifyTime = Date.now().toString().substr(0,10);
    this.save();   
  };
  
  //finish initialization, loading saved data
  if(this.load()) this.loadUpdates();
  this.save();
  if(debugPageList)console.log("PageList:init "+pageKey+":success");
}

//----------PAGE TEXT----------
//Handles page text
function PageText(textKey,pageLists,pocketConnection){ 
  this.PAGE_SIZE_LIMIT = 4 * 1024 * 1024;//limit: 4MB
  this.DEFAULT_PAGE_SIZE = 400; //Number of bytes to send at a time
  
  /**
  *saves page data to local storage
  *return: true if data is saved, false otherwise
  */
  this.save = function(){ 
    try{
      var saveData = {};
      for(var key in this){
        if(key != "currentPage" && key != "pocketConnection" && key != "pageLists")
          saveData[key] = this[key];
      }
      var itemsRemoved = 0;
      while(JSON.stringify(saveData).length > this.PAGE_SIZE_LIMIT){
        saveData.savedPageList.pop();
        itemsRemoved++;
      }
      if(debugPageText && itemsRemoved > 0)
        console.log("SavedPage.save: needed to remove "+itemsRemoved+" items from list");
      localStorage.setItem(textKey,JSON.stringify(saveData));
      if(debugPageText)console.log("SavedPage.save: saved SavedPage of size "+JSON.stringify(saveData).length);
    }catch(err){
      if(debugPageText)console.log("SavedPage.save: error,"+err);
    }
  };
  
  /**
  *Loads list data from local storage
  *returns true if data was found, false if not
  */
  this.load = function(){
  var savedPages = localStorage.getItem(this.textKey);
    if(savedPages){ 
      savedPages = JSON.parse(savedPages);
      if(debugPageText)console.log("SavedPage.load:loaded "+textKey+" from storage");
      for(var key in savedPages){
        this[key] = savedPages[key];
      }
      if(debugPageText)console.log("SavedPage.load: loaded SavedPage of size "+JSON.stringify(this).length);
      return true;
    }
    if(debugPageText)console.log("SavedPage.load:didn't find "+textKey+" in storage");
    return false;
  };
  
   //Initialize values
  this.textKey = textKey;
  this.pageLists = pageLists;
  this.pocketConnection = pocketConnection;
  if(!this.load()){
    this.savedPages = [];
    this.currentPage = {};
    this.save();
  }
  
 
  /**
  *Loads a new currentPage
  *pageNum: the page to load
  */
  this.loadPage = function(pageNum){
    var onload = function(pageLists,savedPage){
      var page = pageLists.getPage(pageNum);
      console.log(JSON.stringify(page));
      if(page) savedPage.initCurrentPage(page,pageNum);
    };
    if(this.pageLists.getPage(pageNum))
      onload(this.pageLists,this);
    else this.pageLists.loadNewPages(pageNum,1,onload,this);
  };
    
  /**
  *Initializes current page data, sending text to pebble
  *page: the new page to load
  *pageNum: page index
  */
  this.initCurrentPage = function(page,pageNum){
    this.currentPage = {};
    //check to see if the page is saved
    var foundPage = this.getBookmarkedPageIndex(page);
    if(foundPage){
      this.currentPage = this.savedPages[foundPage];
      this.currentPage.page = page;
      if(debugPageText)console.log("initCurrentPage: loaded page "+pageNum+" from saved pages");
      if(debugPageText)console.log("initCurrentPage: bookmark is at subpage "+this.currentPage.subpage+" offset "+this.currentPage.offset);
      if(this.currentPage.subpage) this.sendText(this.currentPage.subpage);
      else this.sendText(0);
    }
    else if(page.given_url){//otherwise load the page
      if(debugPageText)console.log("initCurrentPage: loading "+page.given_url);
      this.currentPage.page = page;
      var pageRequest = new XMLHttpRequest();
      var savedPage = this;
      try{
        pageRequest.open("GET", page.given_url, true);
        pageRequest.onload = function(){
          //savedPage.findNextPage(page,this.response);
          savedPage.currentPage.text = savedPage.processPageText(page,this.response + '\n');
          savedPage.currentPage.text = savedPage.textToSubPages(savedPage.currentPage.text);
          savedPage.currentPage.index = pageNum;
          //send first subpage to pebble
          savedPage.sendText(0);
          if(debugPageText)console.log("initCurrentPage: Getting page "+pageNum);
        };
        pageRequest.send();
      }
      catch(err){
        if(debugPageText)console.log("initCurrentPage: error: " + err);
        sendResultMessage(false,"Failed to load page!",OPCODES.loadText);
      }
    }
  };
  
  /**
  *Returns the list of all saved pages, as raw page
  *data
  */
  this.getSavedPageList = function(){
    var pageList = [];
    for(var index in this.savedPages){
      pageList.push(this.savedPages[index].page);
    }
    return pageList;
  };
   
  /**
  *Saves the current page to the list of bookmarked pages
  *subpage: the subpage the reader was on
  *offset: the amount the reader was scrolled past subpage start
  */
  this.bookmarkCurrentPage = function(subpage,offset){
    if(debugPageText)console.log("saving page of size "+ this.currentPage.toString().length + " subpage="+subpage+" offset="+offset);
    if(this.currentPage){
      this.removeCurrentPageBookmark();//remove old bookmarks for the page
      this.currentPage.subpage = subpage;
      this.currentPage.offset = offset;
      this.savedPages.push(this.currentPage);
      this.save();
    }
  };
  
  /**
  *Attempts to find a given page among bookmarked pages,
  *returning the index if it is found
  */
  this.getBookmarkedPageIndex = function(page){
    for(var index in this.savedPages){
      //if(debugPageText)console.log("checking against page:"+JSON.stringify(this.savedPages[index]));
      if(this.savedPages[index].page.item_id == page.item_id)return index;  
    }
  };
  
  /**
  *removes the current page from the list of bookmarked pages
  */
  this.removeCurrentPageBookmark = function(){
    var pageIndex = this.getBookmarkedPageIndex(this.currentPage.page);
    if(pageIndex){ 
      this.savedPages.slice(pageIndex,1); 
      this.currentPage.subpage = null;
      this.currentPage.offset = null;
      this.save();
    }
  };
  
  /**
  *Attempts to find the next page of an article
  */
  this.findNextPage = function(page,pageText){
    var url = page.given_url;
    
    var removeExtension = /(\S*(?=\.\S{3,4}$))/;
    url = url.match(removeExtension);
    if(url)url = url[0];
    else url = page.given_url;
    if(debugPageText)console.log("SHORTENED URL:"+url);
    //var pageMatch = new RegExp("\""+url+".+?\"","g");
    var pageMatch = /.*p.*/g;
    var matches = pageText.match(pageMatch);
    for(var index in matches){
      if(debugPageText)console.log("URL MATCH: "+matches[index]);
    }
  };
  
  /**
  *Parses webpages, attempting to extract only relevant text
  *page: page data from pocket
  *rawText: downloaded html from the page source
  */
  this.processPageText = function(page, rawText){
    if(debugPageText)console.log("request status:" +this.statusText);
    var any = "(\\S|\\s)";
    function removeTag(text,tag){   
    var regex = new RegExp("(<"+ tag + any +"*?>"+any+"*?<\/" + tag + any + "*?>(?=" + any + "))","m","i");
    var newText = rawText;
    var substr;
    do{
      substr = newText;
      //watch out for nested matches
      while(substr.match(regex) !== null){
        substr = (substr.match(regex))[0];
      }
      if(substr == newText)substr = "";
        newText = newText.replace(substr,"");
      }while(substr !== "");
    if(debugPageText)console.log("removing "+tag+" removed "+ (rawText.length-newText.length) +" characters");
      return newText;
    }
    rawText = removeTag(rawText,"head");
    rawText = removeTag(rawText,"script");
    rawText = removeTag(rawText,"style");
    rawText = removeTag(rawText,"nav");
    rawText = removeTag(rawText,"ul");    
    rawText = rawText.replace(/<[^<>]*>/g,"");//remove tags
    rawText = rawText.replace(/(\t|\f| )+/g," ");//condense spaces
    rawText = rawText.replace(/\n\s+/g,"\n");//condense line breaks
    if(debugPageText)console.log("removing null, strlen: "+ rawText.length);
    rawText = rawText.replace(/\x00/,"");//remove null characters
    if(debugPageText)console.log("removed null, strlen: "+ rawText.length);
    rawText = rawText.replace(/&#8217;/g,"'");
    rawText = rawText.replace(/(&#8220;)|(&#8221;)|(&ldquo)|(&rdquo)/g,"\"");    
    //anything still remaining before pocket's excerpt is probably junk we can discard
    if(page.excerpt){
      var escapedExcerpt = page.excerpt.replace(/[-\/\\^$*+?.()|[\]{}]/g, '\\$&');
      if(debugPageText)console.log("excerpt:"+escapedExcerpt);
      var len = rawText.length;
      if(debugPageText)console.log("remaining text length:"+len);
      var regex = new RegExp("("+escapedExcerpt+")"+any+"*");
      if(rawText.match(regex))rawText = rawText.match(regex)[0];
        if(debugPageText)console.log("removed " + (len - rawText.length) + " characters before excerpt");
      }
    if(debugPageText)console.log("processPageText:page text processed");
      return rawText;  
  };
  
  /**
  *Breaks the page text into an array of subpages of DEFAULT_PAGE_SIZE or less
  *pageText:the page to process
  *return: the page as an array of subpages
  */
  this.textToSubPages = function(pageText){
    console.log("pageText size:"+pageText.length);
    var index = 0;
    var pageArray = [];
    var pageNum = 0;
    while(index <= pageText.length){
      var subPage = pageText.substr(index,this.DEFAULT_PAGE_SIZE);
      index += subPage.length;
      if(subPage.length === 0)break;
      if(debugPageText)console.log("Index:"+index);
      //trim page until it ends in whitespace
      if(subPage.match(/\s/)){//don't bother if there's no whitespace
         while(!subPage.match(/^(\s|\S)*\s$/) && subPage.length >1){
          subPage = subPage.substr(0,subPage.length-1);
          index --;
        }
      }
      if(debugPageText)console.log("subpage "+pageNum+": "+subPage.length+" chars at index "+index);
      pageNum++;
      pageArray.push(subPage);
    }
    return pageArray;
  };
  
  /**
  *Sends the requested subpage to pebble
  *index: index of the subpage to send back to pebble
  */
  this.sendText = function(index){
    if(this.currentPage.text.length <= index){
      if(debugPageText)console.log("error:requested subpage at "+index+", but pagecount="+this.currentPage.text.length);
      return;
    }
    var textBlock;
    textBlock = this.currentPage.text[index];
    if(debugPageText)console.log("requested subpage at "+index);
    if(debugPageText)console.log("sending " + textBlock.length +" characters,"+textBlock);
    var appMsg = {};
    appMsg.message_code = JS_MESSAGE_CODES.sendingPageText;
    appMsg.message_text = textBlock;
    appMsg.index = index;
    appMsg.item_count = this.currentPage.text.length;
    appMsg.favorite = parseInt(this.currentPage.page.favorite,10);
    appMsg.page_state = parseInt(this.currentPage.page.status,10);
    if(this.currentPage.subpage == index && 
       this.currentPage.offset !== undefined)
      appMsg.scroll_offset = this.currentPage.offset;
    Pebble.sendAppMessage(appMsg);
    console.log("page_size: "+this.currentPage.text.length+" fave_status:"+this.currentPage.page.favorite+" page_state:"+this.currentPage.page.status);
  };

  /**
  *Sends a pocket request to perform some action on the 
  *current page
  *action: the action to apply
  *callback: callback function to run if action succeeds
  */
  this.modifyPageData = function(action,callback){
    var request = {};
    request.actions = action;
    var modCallback = function(connection,pageText){
      var response = JSON.parse(connection.response);
      if(response.status == 1){
        if(debugPageText)console.log("modifyData: action succeeded");
        if(callback) callback();
      }
      else sendResultMessage(false,action[0].action+" page failed",OPCODES.pageAction);
    };
    this.pocketConnection.pocketModify(request,modCallback,this.pageLists);
    if(debugPageText)console.log("modifyPageData: Sending action "+JSON.stringify(action));
  };
  
  /**
  *Archives or re-adds the current page
  */
  this.archiveOrReAdd = function(){
    if(this.currentPage){
      var page = this.currentPage.page;
      var pageLists = this.pageLists;
      var actionType;
      if(page.status == '0'){
        if(debugPageText)console.log('archiveOrReAdd: archiving current page');
        actionType = "archive";
      }
      else{
        if(debugPageText)console.log('archiveOrReAdd: re-adding current page');
        actionType = "readd";
      }
      var archiveAction = [{"action":actionType,
                          "item_id":page.item_id,
                          "time":pocketTime()}];
      //run this when (un)archive succeeds
      var callback = function(pageText){
        var message;
        if(page.status == '0'){//Page was archived
          message = "Archived \""+page.resolved_title+"\"";
          page.status = 1;
        }
        else{//Page was re-added
          message = "Re-added \""+page.resolved_title+"\"";
          page.status = 0;
        }
        pageLists.updatePage(page);
        pageLists.sortAllLists();
        sendResultMessage(true,message,OPCODES.toggleArchive);
      };
      this.modifyPageData(archiveAction,callback);
    }
  };
  
  /**
  *favorites or unfavorites the current page
  */
  this.favoriteToggle = function(){
    if(this.currentPage){
      var actionType;
      if(this.currentPage.page.favorite == '0'){
        if(debugPageText)console.log('favoriteToggle: favoriting current page');
        actionType = "favorite";
      }
      else{
        if(debugPageText)console.log('archiveOrReAdd: unfavoriting current page');
        actionType = "unfavorite";
      }
      var favoriteAction = [{"action":actionType,
                          "item_id":this.currentPage.page.item_id,
                          "time":pocketTime()}];
      this.modifyPageData(favoriteAction);
    }
  };
}



// Listen for when the watchface is opened
Pebble.addEventListener('ready', 
  function(e) {
    connection = new PocketConnection("POCKET_CONNECTION");
    savedPageLists = new PageLists("SAVED_PAGES",connection);
    savedPage = new PageText("PAGE",savedPageLists,connection);
    Pebble.sendAppMessage({'message_code': JS_MESSAGE_CODES.JSInitialized});
    if(!connection.access_token)connection.getAccessToken(); 
  });

// Listen for when an AppMessage is received
Pebble.addEventListener('appmessage',
  function(e) {
    if(debug)console.log('appmessage: AppMessage received!');
    if(debug)console.log('message content: '+JSON.stringify(e.payload));
    if(e.payload.page_state !== undefined){
      savedPageLists.pageState = PAGE_STATE_VALUES[e.payload.page_state];
      console.log('page state is now '+ savedPageLists.pageState);
    }
    if(e.payload.sort_order !== undefined){
      var newOrder = SORT_ORDER_VALUES[e.payload.sort_order];
      if(newOrder != savedPageLists.sortOrder){
        savedPageLists.sortOrder = SORT_ORDER_VALUES[e.payload.sort_order];
        console.log('sort order is now '+savedPageLists.sortOrder+", clearing old page lists");
        savedPageLists.clearLists();
      }
    }
    if(e.payload.favorite !== undefined){
      savedPageLists.favoriteStatus = FAVE_STATUS_VALUES[e.payload.favorite];
      console.log('favorite status is now '+savedPageLists.favoriteStatus);
    }
    if(e.payload.message_code == PEBBLE_MESSAGE_CODES.requestingPageTitles){
      if(debug)console.log('appmessage: Pebble requested ' + e.payload.item_count + " titles starting at " + e.payload.index );
      savedPageLists.pagesToPebble(e.payload.index,e.payload.item_count);
    }
    else if(e.payload.message_code == PEBBLE_MESSAGE_CODES.loadPage){
      
      if(debug)console.log('appmessage: Pebble requested page at index ' + e.payload.index);
      savedPage.loadPage(e.payload.index);
    }
    else if(e.payload.message_code == PEBBLE_MESSAGE_CODES.getPageText){
      if(savedPage.currentPage){
        if(debug)console.log('appmessage: Sending page text at subpage ' + e.payload.index);
        savedPage.sendText(e.payload.index);
      }
      else if(debug)console.log('appmessage: page text requested, but no page is loaded');
    }
    else if(e.payload.message_code == PEBBLE_MESSAGE_CODES.archivePage){
      savedPage.archiveOrReAdd();
    }
    else if(e.payload.message_code == PEBBLE_MESSAGE_CODES.deletePage){
      if(savedPage.currentPage){
        if(debug)console.log('appmessage: deleting current page');
        var deleteAction =  [{"action":"delete",
                              "item_id":savedPage.currentPage.page.item_id,
                              "time":pocketTime()}];
        savedPage.modifyPageData(deleteAction);
      }
    }
    else if(e.payload.message_code == PEBBLE_MESSAGE_CODES.favoritePage){
      savedPage.favoriteToggle();
    }
    else if(e.payload.message_code == PEBBLE_MESSAGE_CODES.clearLocalStorage){
      console.log("clearing local storage!");
      localStorage.clear();
      savedPageLists = new PageLists("SAVED_PAGES",connection);
      savedPage = new PageText("PAGE",savedPageLists,connection);
      sendResultMessage(true,"Local storage cleared.",OPCODES.pageAction);
    }
    else if(e.payload.message_code == PEBBLE_MESSAGE_CODES.updateTitles){
      var callback = function(){
          Pebble.sendAppMessage({'message_code':JS_MESSAGE_CODES.resetPageData});
      };
      savedPageLists.loadUpdates(callback);
    }
    else if(e.payload.message_code == PEBBLE_MESSAGE_CODES.bookmarkPage){
      if(debug)console.log('appmessage: bookmarking current page');
      var subpage = e.payload.index;
      var scrollOffset = e.payload.scroll_offset;
      savedPage.bookmarkCurrentPage(subpage,scrollOffset);
    }
    else if(e.payload.message_code == PEBBLE_MESSAGE_CODES.removeBookmark){
      if(debug)console.log('appmessage: removing current page bookmark');
      savedPage.removeCurrentPageBookmark();
    }
  });
          
//Runs when the config page is opened
Pebble.addEventListener('showConfiguration', function(e) {
  connection.openConfig();
});

//Runs when the config page is closed
Pebble.addEventListener('webviewclosed', function(e) {
  // Decode and parse config data as JSON
  connection.getAccessToken(); 
});

