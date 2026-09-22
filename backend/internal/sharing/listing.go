package sharing

import (
	"context"
	"fmt"
	"sort"

	"mindarchy/backend/internal/couch"
	"mindarchy/backend/internal/protocol"
)

type accountLister interface {
	ListAccountMaps(context.Context, protocol.AccountID, string, int) ([]couch.MapSummary, string, error)
}

func (s *Service) ListPage(ctx context.Context, account protocol.AccountID, after string, limit int) ([]couch.MapSummary, string, error) {
	if limit < 1 || limit > 200 {
		return nil, "", fmt.Errorf("invalid page size")
	}
	if indexed, ok := s.persistence.(accountLister); ok {
		return indexed.ListAccountMaps(ctx, account, after, limit)
	}
	// In-memory test/demo service only. Persistent production storage must supply
	// the index; do not silently fall back to scanning the entire database.
	if s.persistence != nil {
		return nil, "", fmt.Errorf("membership index unavailable")
	}
	s.mu.RLock()
	defer s.mu.RUnlock()
	result := []couch.MapSummary{}
	for _, entry := range s.maps {
		if role := entry.ACL[account]; role != "" && string(entry.ID) > after {
			result = append(result, couch.MapSummary{ID: entry.ID, Name: entry.Name, Owner: entry.Owner, ACL: map[string]string{string(account): role}})
		}
	}
	sort.Slice(result, func(i, j int) bool { return result[i].ID < result[j].ID })
	next := ""
	if len(result) > limit {
		result = result[:limit]
		next = string(result[len(result)-1].ID)
	}
	return result, next, nil
}
