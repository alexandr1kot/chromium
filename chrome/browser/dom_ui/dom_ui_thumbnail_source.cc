// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/dom_ui/dom_ui_thumbnail_source.h"

#include "app/resource_bundle.h"
#include "base/command_line.h"
#include "base/gfx/jpeg_codec.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/thumbnail_store.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/url_constants.h"
#include "googleurl/src/gurl.h"
#include "grit/theme_resources.h"
#include "third_party/skia/include/core/SkBitmap.h"

DOMUIThumbnailSource::DOMUIThumbnailSource(Profile* profile)
    : DataSource(chrome::kChromeUIThumbnailPath, MessageLoop::current()),
      profile_(profile),
      store_(profile->GetThumbnailStore()) {
}

DOMUIThumbnailSource::~DOMUIThumbnailSource() {
  store_->CancelPendingRequests(pending_requests_);
}

void DOMUIThumbnailSource::StartDataRequest(const std::string& path,
                                            int request_id) {
  if (CommandLine::ForCurrentProcess()->HasSwitch(switches::kThumbnailStore)) {
    RefCountedBytes* data = NULL;

    ThumbnailStore::GetStatus res = store_->GetPageThumbnail(GURL(path), &data);
    if (res == ThumbnailStore::SUCCESS) {
      // Got the thumbnail.
      SendResponse(request_id, data);
    } else if (res == ThumbnailStore::FAIL) {
      // Don't have the thumbnail so return the default thumbnail.
      if (!default_thumbnail_.get()) {
        default_thumbnail_ = new RefCountedBytes;
        ResourceBundle::GetSharedInstance().LoadImageResourceBytes(
            IDR_DEFAULT_THUMBNAIL, &default_thumbnail_->data);
      }
      SendResponse(request_id, default_thumbnail_);
    } else if (res == ThumbnailStore::ASYNC) {
      // Getting the redirect list for the url. Will return thumbnail later.
      ThumbnailStore::ThumbnailDataCallback* cb =
          NewCallback(this, &DOMUIThumbnailSource::ReturnData);
      pending_requests_.insert(request_id);
      store_->GetPageThumbnailAsync(GURL(path), request_id, cb);
    }
    return;
  }  // end --thumbnail-store switch

  HistoryService* hs = profile_->GetHistoryService(Profile::EXPLICIT_ACCESS);
  if (hs) {
    HistoryService::Handle handle = hs->GetPageThumbnail(
        GURL(path),
        &cancelable_consumer_,
        NewCallback(this, &DOMUIThumbnailSource::OnThumbnailDataAvailable));
    // Attach the ChromeURLDataManager request ID to the history request.
    cancelable_consumer_.SetClientData(hs, handle, request_id);
  } else {
    // Tell the caller that no thumbnail is available.
    SendResponse(request_id, NULL);
  }
}

void DOMUIThumbnailSource::ReturnData(int request_id,
                                      scoped_refptr<RefCountedBytes> data) {
  pending_requests_.erase(request_id);
  if (data.get() && !data->data.empty()) {
    SendResponse(request_id, data);
  } else {
    if (!default_thumbnail_.get()) {
      default_thumbnail_ = new RefCountedBytes;
      ResourceBundle::GetSharedInstance().LoadImageResourceBytes(
          IDR_DEFAULT_THUMBNAIL, &default_thumbnail_->data);
    }

    SendResponse(request_id, default_thumbnail_);
  }
}

void DOMUIThumbnailSource::OnThumbnailDataAvailable(
    HistoryService::Handle request_handle,
    scoped_refptr<RefCountedBytes> data) {
  HistoryService* hs =
      profile_->GetHistoryService(Profile::EXPLICIT_ACCESS);
  int request_id = cancelable_consumer_.GetClientData(hs, request_handle);
  // Forward the data along to the networking system.
  if (data.get() && !data->data.empty()) {
    SendResponse(request_id, data);
  } else {
    if (!default_thumbnail_.get()) {
      default_thumbnail_ = new RefCountedBytes;
      ResourceBundle::GetSharedInstance().LoadImageResourceBytes(
          IDR_DEFAULT_THUMBNAIL, &default_thumbnail_->data);
    }

    SendResponse(request_id, default_thumbnail_);
  }
}
