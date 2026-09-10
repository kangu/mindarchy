#import <Cocoa/Cocoa.h>
#import <Quartz/Quartz.h>
#import <UniformTypeIdentifiers/UniformTypeIdentifiers.h>
#include "engine.h"
#include "preview.h"
#include <QBuffer>
#include <QGuiApplication>

@interface OMMPreviewProvider : QLPreviewProvider <QLPreviewingController>
@end
@implementation OMMPreviewProvider
- (void)providePreviewForFileRequest:(QLFilePreviewRequest *)request
                 completionHandler:(void (^)(QLPreviewReply *, NSError *))handler {
    // Qt's font system must be initialized on the main thread. Offscreen QPA
    // leaves the extension host's NSApplication and windows under Apple's control.
    dispatch_async(dispatch_get_main_queue(), ^{
        if(!QGuiApplication::instance()) {
            qputenv("QT_QPA_PLATFORM","offscreen");
            NSString *plugins=[[[NSBundle mainBundle] bundlePath] stringByAppendingPathComponent:@"Contents/PlugIns/platforms"];
            if([[NSFileManager defaultManager] fileExistsAtPath:plugins])
                qputenv("QT_QPA_PLATFORM_PLUGIN_PATH",[plugins fileSystemRepresentation]);
            static int argc=1;
            static char name[]="OMMPreview";
            static char *argv[]={name,nullptr};
            // Retained for the extension process lifetime, including later requests.
            new QGuiApplication(argc,argv);
        }
        NSURL *url=request.fileURL;
        BOOL scoped=[url startAccessingSecurityScopedResource];
        Engine engine;
        bool loaded=engine.open(QString::fromUtf8(url.fileSystemRepresentation));
        if(scoped) [url stopAccessingSecurityScopedResource];
        if(!loaded) {
            NSString *message=[NSString stringWithUTF8String:engine.error().toUtf8().constData()];
            handler(nil,[NSError errorWithDomain:@"org.mindarchy.omm" code:1
                userInfo:@{NSLocalizedDescriptionKey:message}]);
            return;
        }
        auto image=renderMapPreview(engine,QSize(2000,1400));
        QByteArray bytes; QBuffer buffer(&bytes); buffer.open(QIODevice::WriteOnly); image.save(&buffer,"PNG");
        NSData *data=[NSData dataWithBytes:bytes.constData() length:bytes.size()];
        QLPreviewReply *reply=[[QLPreviewReply alloc] initWithDataOfContentType:UTTypePNG
            contentSize:CGSizeMake(image.width(),image.height())
            dataCreationBlock:^NSData *(QLPreviewReply *, NSError **) { return data; }];
        handler(reply,nil);
    });
}
@end
