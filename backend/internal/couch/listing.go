package couch

import (
	"bytes"
	"context"
	"encoding/json"
	"fmt"
	"net/http"
	"net/url"

	"mindarchy/backend/internal/protocol"
)

// Summaries deliberately omit map contents and other members' identities.
type MapSummary struct {
	ID    protocol.MapID
	Name  string
	Owner protocol.AccountID
	ACL   map[string]string
}

// Legacy heads store JSON as base64 payload. Decode only head records during
// indexing; newer heads expose the same fields directly. The view is versioned.
const membershipMap = `function(doc) {
 if (doc.kind !== "head" || doc._id.slice(0,4) !== "map:" || doc._id.slice(-5) !== ":head") return;
 var h = doc.head;
 if (!h) {
  var chars="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/", bits=0, count=0, escaped="";
  for(var i=0;i<doc.payload.length;i++) {
   var n=chars.indexOf(doc.payload.charAt(i)); if(n<0) break;
   bits=(bits<<6)|n; count+=6;
   if(count>=8) { count-=8; var b=(bits>>count)&255; escaped+="%"+("0"+b.toString(16)).slice(-2); }
  }
  h=JSON.parse(decodeURIComponent(escaped));
 }
 if(h.deleted || !h.acl) return;
 var id=doc._id.slice(4,-5), owner="";
 Object.keys(h.acl).forEach(function(a){if(h.acl[a]==="owner") owner=a;});
 Object.keys(h.acl).forEach(function(a){
  var acl={};acl[a]=h.acl[a];
  emit([a,id],{ID:id,Name:h.name||id,Owner:owner,ACL:acl});
 });
}`

func (s *StoreClient) EnsureIndexes(ctx context.Context) error {
	endpoint := s.base + "/" + url.PathEscape(s.db) + "/_design/mindarchy-membership-v1"
	req, err := http.NewRequestWithContext(ctx, http.MethodGet, endpoint, nil)
	if err != nil {
		return err
	}
	if s.user != "" {
		req.SetBasicAuth(s.user, s.pass)
	}
	res, err := s.client.Do(req)
	if err != nil {
		return err
	}
	res.Body.Close()
	if res.StatusCode == http.StatusOK {
		return nil
	}
	if res.StatusCode != http.StatusNotFound {
		return fmt.Errorf("read membership index: %d", res.StatusCode)
	}
	body, _ := json.Marshal(map[string]any{"_id": "_design/mindarchy-membership-v1", "language": "javascript", "views": map[string]any{"by_account": map[string]string{"map": membershipMap}}})
	req, err = http.NewRequestWithContext(ctx, http.MethodPut, endpoint, bytes.NewReader(body))
	if err != nil {
		return err
	}
	req.Header.Set("Content-Type", "application/json")
	if s.user != "" {
		req.SetBasicAuth(s.user, s.pass)
	}
	res, err = s.client.Do(req)
	if err != nil {
		return err
	}
	defer res.Body.Close()
	// Another starting process may have installed this immutable version already.
	if res.StatusCode == http.StatusConflict || res.StatusCode == http.StatusCreated || res.StatusCode == http.StatusAccepted {
		return nil
	}
	return fmt.Errorf("install membership index: %d", res.StatusCode)
}

func (s *StoreClient) ListAccountMaps(ctx context.Context, account protocol.AccountID, after string, limit int) ([]MapSummary, string, error) {
	if limit < 1 || limit > 200 {
		return nil, "", fmt.Errorf("invalid page size")
	}
	// View keys have two elements. A third element sorts after the exact
	// boundary key without relying on string sentinels or skip (which loses a
	// row when the boundary document is deleted between requests).
	startKey := []any{string(account)}
	if after != "" {
		startKey = []any{string(account), after, map[string]any{}}
	}
	start, _ := json.Marshal(startKey)
	end, _ := json.Marshal([]any{string(account), map[string]any{}})
	query := url.Values{"startkey": {string(start)}, "endkey": {string(end)}, "limit": {fmt.Sprint(limit + 1)}, "reduce": {"false"}}
	endpoint := s.base + "/" + url.PathEscape(s.db) + "/_design/mindarchy-membership-v1/_view/by_account?" + query.Encode()
	req, err := http.NewRequestWithContext(ctx, http.MethodGet, endpoint, nil)
	if err != nil {
		return nil, "", err
	}
	if s.user != "" {
		req.SetBasicAuth(s.user, s.pass)
	}
	res, err := s.client.Do(req)
	if err != nil {
		return nil, "", err
	}
	defer res.Body.Close()
	if res.StatusCode != http.StatusOK {
		return nil, "", fmt.Errorf("list membership: %d", res.StatusCode)
	}
	var result struct {
		Rows []struct {
			Value MapSummary `json:"value"`
		} `json:"rows"`
	}
	if err := json.NewDecoder(res.Body).Decode(&result); err != nil {
		return nil, "", err
	}
	page := make([]MapSummary, 0, limit)
	for i, row := range result.Rows {
		if i == limit {
			break
		}
		page = append(page, row.Value)
	}
	next := ""
	if len(result.Rows) > limit {
		next = string(page[len(page)-1].ID)
	}
	return page, next, nil
}
