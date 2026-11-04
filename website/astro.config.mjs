import { defineConfig } from "astro/config";
import starlight from "@astrojs/starlight";

export default defineConfig({
  site: "https://kdal.vercel.app",
  integrations: [
    starlight({
      title: "KDAL",
      description:
        "Kernel Device Abstraction Layer - write portable Linux device drivers once, run everywhere.",
      logo: {
        light: "./src/assets/logo-light.svg",
        dark: "./src/assets/logo-dark.svg",
        replacesTitle: false,
      },
      social: {
        github: "https://github.com/NguyenTrongPhuc552003/kdal",
      },
      editLink: {
        baseUrl:
          "https://github.com/NguyenTrongPhuc552003/kdal/edit/main/website/",
      },
      customCss: ["./src/styles/custom.css"],
      head: [
        {
          tag: "meta",
          attrs: {
            property: "og:image",
            content: "https://kdal.vercel.app/og.png",
          },
        },
      ],
      sidebar: [
        {
          label: "Getting Started",
          items: [
            { label: "Introduction", slug: "getting-started/introduction" },
            { label: "Installation", slug: "getting-started/installation" },
            { label: "Quick Start", slug: "getting-started/quickstart" },
          ],
        },
        {
          label: "Language Guide",
          items: [
            { label: "Overview", slug: "language/overview" },
            { label: "Device Headers (.kdh)", slug: "language/device-headers" },
            { label: "Driver Code (.kdc)", slug: "language/driver-code" },
            { label: "Standard Library", slug: "language/stdlib" },
            { label: "Language Specification", slug: "language/spec" },
            { label: "Grammar Reference", slug: "language/grammar" },
          ],
        },
        {
          label: "Toolchain",
          items: [
            { label: "Compiler (kdalc)", slug: "toolchain/compiler" },
            { label: "CLI (kdality)", slug: "toolchain/kdality" },
            { label: "Editor Support", slug: "toolchain/editors" },
          ],
        },
        {
          label: "Kernel Framework",
          items: [
            { label: "Architecture", slug: "kernel/architecture" },
            { label: "Writing Drivers", slug: "kernel/drivers" },
            { label: "API Reference", slug: "kernel/api" },
            { label: "Porting Guide", slug: "kernel/porting" },
          ],
        },
        {
          label: "Development",
          items: [
            { label: "Contributing", slug: "development/contributing" },
            { label: "Building from Source", slug: "development/building" },
            { label: "Testing", slug: "development/testing" },
            { label: "Design Philosophy", slug: "development/design" },
          ],
        },
        {
          label: "Reference",
          items: [
            { label: "FAQ", slug: "reference/faq" },
            { label: "Changelog", slug: "reference/changelog" },
            { label: "Performance", slug: "reference/performance" },
            { label: "Upstreaming Plan", slug: "reference/upstreaming" },
            { label: "Thesis Mapping", slug: "reference/thesis" },
          ],
        },
      ],
    }),
  ],
});
