#import <AppKit/AppKit.h>
#import <UniformTypeIdentifiers/UniformTypeIdentifiers.h>
#include <MadDisplayStudio/file_dialog.hpp>
namespace madstudio {
std::string chooseImageFile() {
  @autoreleasepool {
    NSOpenPanel* panel = [NSOpenPanel openPanel];
    [panel setCanChooseDirectories:NO]; [panel setAllowsMultipleSelection:NO];
    [panel setAllowedContentTypes:@[UTTypePNG,UTTypeJPEG,UTTypeSVG]];
    if ([panel runModal] == NSModalResponseOK) return [[[panel URL] path] UTF8String];
  }
  return {};
}
bool editUnicodeText(const std::string& current,std::string& result) {
  @autoreleasepool {
    NSAlert* dialog = [[NSAlert alloc] init]; [dialog setMessageText:@"Text"];
    [dialog addButtonWithTitle:@"Apply"]; [dialog addButtonWithTitle:@"Cancel"];
    NSScrollView* scroll = [[NSScrollView alloc] initWithFrame:NSMakeRect(0,0,560,240)];
    [scroll setHasVerticalScroller:YES];
    NSTextView* editor = [[NSTextView alloc] initWithFrame:NSMakeRect(0,0,540,240)];
    [editor setRichText:NO]; [editor setFont:[NSFont systemFontOfSize:18]];
    [editor setString:[[NSString alloc] initWithBytes:current.data() length:current.size() encoding:NSUTF8StringEncoding]];
    [scroll setDocumentView:editor]; [dialog setAccessoryView:scroll];
    [[dialog window] setInitialFirstResponder:editor];
    if ([dialog runModal] != NSAlertFirstButtonReturn) return false;
    NSString* value = [editor string]; result.assign([value UTF8String],[value lengthOfBytesUsingEncoding:NSUTF8StringEncoding]); return true;
  }
}

}
