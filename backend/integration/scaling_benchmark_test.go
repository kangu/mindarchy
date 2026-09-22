package integration

import (
	"context"
	"encoding/json"
	"fmt"
	"mindarchy/backend/internal/couch"
	"mindarchy/backend/internal/sharing"
	"testing"
)

// CPU-only synthetic benchmark. This is not a CouchDB throughput claim.
func BenchmarkCurrentMapRead(b *testing.B) {
	for _, history := range []int{1, 1000, 10000} {
		b.Run(fmt.Sprint(history), func(b *testing.B) {
			ctx := context.Background()
			s := &countingStore{memCASStore: newMemCASStore()}
			svc := sharing.NewPersistentService(s)
			entry := svc.Create("owner", []byte(`{}`), "map")
			if err := svc.PersistMap(entry); err != nil {
				b.Fatal(err)
			}
			head, _ := s.LoadHead(ctx, entry.ID)
			for i := 1; i <= history; i++ {
				id := fmt.Sprintf("map:%s:batch:%d", entry.ID, i)
				data, _ := json.Marshal(couch.Batch{ID: id, Seq: uint64(i), Parent: head.BatchID, Changes: []byte(`{"value":"latest"}`)})
				s.PutImmutable(ctx, id, data)
				head.BatchID = id
				head.Seq = uint64(i)
			}
			s.CompareAndSwapHead(ctx, entry.ID, head.Rev, head)
			s.reads = 0
			b.ResetTimer()
			for i := 0; i < b.N; i++ {
				if _, err := sharing.NewPersistentService(s).Get("owner", entry.ID); err != nil {
					b.Fatal(err)
				}
			}
			b.StopTimer()
			b.ReportMetric(float64(s.reads)/float64(b.N), "immutable-reads/op")
		})
	}
}
